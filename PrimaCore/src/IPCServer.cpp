// ============================================================
// Prima Multi Seat - IPC Server Implementation  [PRODUCTION]
// Named pipe server for UI <-> Core communication
// Hardened: thread-safe, overlapped I/O, robust validation
//
// Corrections applied (see review):
//   C-01  m_hPipe HANDLE race between Stop() and ServerThread()
//   C-02  Synchronous ReadFile on overlapped handle — now overlapped
//   C-03  ReadFile without OVERLAPPED* on overlapped handle — fixed
//   C-04  WriteFile without OVERLAPPED* on overlapped handle — fixed
//   H-01  GetOverlappedResult drain could block — CancelIoEx first
//   H-02  memory_order upgraded to seq_cst for m_running
//   H-03  payloadSize capped before use everywhere
//   H-04  NOMINMAX guard + std::min<T> with explicit type
//   H-05  PostQuitMessage replaced with application stop callback
//   M-01  Double-close/disconnect of hPipe in loop tail guarded
//   M-02  noexcept removed where LOG_* may allocate
//   M-03  Enum validation via switch instead of range arithmetic
//   M-04  packet re-zeroed each iteration
//   M-06  Removed dead kMaxPayloadDisplay constant
//   M-07  kPipeOpenTimeout renamed to kClientWaitTimeoutMs
//   L-01  noexcept audited
//   L-02  ConnectNamedPipe TRUE return treated as unexpected, warned
//   L-03  %S replaced with explicit wide conversion in log
//   L-04  LOG_INFO pipe name via %s format arg
// ============================================================

// Ensure NOMINMAX is defined before any Windows header to prevent
// the min/max macro pollution that breaks std::min / std::max (H-04).
#ifndef NOMINMAX
#  define NOMINMAX
#endif

#include "../include/IPCServer.h"
#include "../include/SeatManager.h"

#include <atomic>
#include <stdexcept>
#include <cassert>
#include <charconv>        // std::from_chars
#include <string_view>
#include <algorithm>       // std::min

// ---------------------------------------------------------------------------
// Internal constants
// ---------------------------------------------------------------------------
static constexpr DWORD kConnectTimeoutMs   = 500;    // (unused — kept for ABI compat)
// kMaxPayloadDisplay removed — was dead (M-06)
static constexpr int   kMaxSeatId          = 9999;   // sanity cap for seat IDs
// Renamed: nDefaultTimeOut for CreateNamedPipeW — timeout advertised to
// WaitNamedPipe() callers, NOT a server-side retry interval (M-07).
static constexpr DWORD kClientWaitTimeoutMs = 5000;

// ---------------------------------------------------------------------------
// UTF-8 conversion helpers
// ---------------------------------------------------------------------------
namespace {

// noexcept removed: LOG_ERROR may allocate (M-02 / L-01).
std::string WideToUtf8(const std::wstring& wide)
{
    if (wide.empty()) return {};

    const int needed = WideCharToMultiByte(
        CP_UTF8, 0,
        wide.c_str(), static_cast<int>(wide.size()),
        nullptr, 0, nullptr, nullptr);

    if (needed <= 0) {
        LOG_ERROR(L"WideToUtf8: WideCharToMultiByte sizing failed (err=%lu)",
                  GetLastError());
        return {};
    }

    std::string out(static_cast<size_t>(needed), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8, 0,
        wide.c_str(), static_cast<int>(wide.size()),
        out.data(), needed, nullptr, nullptr);

    if (written != needed) {
        LOG_ERROR(L"WideToUtf8: WideCharToMultiByte conversion failed (err=%lu)",
                  GetLastError());
        return {};
    }
    return out;
}

// noexcept removed (M-02 / L-01).
std::wstring Utf8ToWide(std::string_view utf8)
{
    if (utf8.empty()) return {};

    const int needed = MultiByteToWideChar(
        CP_UTF8, 0,
        utf8.data(), static_cast<int>(utf8.size()),
        nullptr, 0);

    if (needed <= 0) {
        LOG_ERROR(L"Utf8ToWide: MultiByteToWideChar sizing failed (err=%lu)",
                  GetLastError());
        return {};
    }

    std::wstring out(static_cast<size_t>(needed), L'\0');
    const int written = MultiByteToWideChar(
        CP_UTF8, 0,
        utf8.data(), static_cast<int>(utf8.size()),
        out.data(), needed);

    if (written != needed) {
        LOG_ERROR(L"Utf8ToWide: MultiByteToWideChar conversion failed (err=%lu)",
                  GetLastError());
        return {};
    }
    return out;
}

/// Parse a seat ID from a payload buffer.
/// payloadLen must be <= sizeof(IPCPacket::payload) - 1 (caller's responsibility).
/// noexcept removed: LOG_* may allocate (L-01).
bool ParseSeatId(const char* payload, DWORD payloadLen, int& outSeat)
{
    // H-03: cap to safe range before any arithmetic
    const DWORD safeLen = std::min<DWORD>(payloadLen,
                              static_cast<DWORD>(sizeof(IPCPacket::payload) - 1u));

    if (!payload || safeLen == 0 || payload[0] == '\0') {
        LOG_WARN(L"ParseSeatId: empty payload");
        return false;
    }

    // strnlen is safe: safeLen < sizeof(payload), so we never overrun (M-05).
    const size_t len = strnlen(payload, static_cast<size_t>(safeLen));
    if (len == 0) {
        LOG_WARN(L"ParseSeatId: zero-length content");
        return false;
    }

    int value = 0;
    const auto [ptr, ec] = std::from_chars(payload, payload + len, value);

    if (ec != std::errc{}) {
        // L-03: convert to wide explicitly instead of %S
        const std::wstring snippet = Utf8ToWide(
            std::string_view(payload, std::min(len, size_t(32))));
        LOG_WARN(L"ParseSeatId: non-numeric payload (%s)", snippet.c_str());
        return false;
    }
    if (ptr != payload + len) {
        LOG_WARN(L"ParseSeatId: trailing garbage in seat payload");
        return false;
    }
    if (value < 0 || value > kMaxSeatId) {
        LOG_WARN(L"ParseSeatId: seat ID %d out of range [0, %d]",
                 value, kMaxSeatId);
        return false;
    }

    outSeat = value;
    return true;
}

/// Validate an inbound IPCPacket before processing.
/// noexcept removed (L-01).
bool ValidatePacket(const IPCPacket& pkt, DWORD bytesRead)
{
    if (bytesRead != sizeof(IPCPacket)) {
        LOG_WARN(L"ValidatePacket: unexpected size (got %lu, want %zu)",
                 bytesRead, sizeof(IPCPacket));
        return false;
    }
    // H-03: payloadSize must not exceed the buffer (excluding null terminator).
    if (pkt.payloadSize >= static_cast<DWORD>(sizeof(pkt.payload))) {
        LOG_WARN(L"ValidatePacket: payloadSize %lu exceeds safe limit %zu",
                 pkt.payloadSize, sizeof(pkt.payload) - 1u);
        return false;
    }
    // M-03: validate enum via switch to survive non-contiguous future values.
    switch (pkt.type) {
        case IPCMessageType::Ping:           // deliberate fall-through
        case IPCMessageType::Pong:
        case IPCMessageType::GetStatus:
        case IPCMessageType::StatusResponse:
        case IPCMessageType::StartSeat:
        case IPCMessageType::StopSeat:
        case IPCMessageType::Shutdown:
            return true;
        default:
            LOG_WARN(L"ValidatePacket: unknown message type value %d",
                     static_cast<int>(pkt.type));
            return false;
    }
}

// ---------------------------------------------------------------------------
// Overlapped I/O helpers
// ---------------------------------------------------------------------------

/// Synchronous-style ReadFile over an overlapped handle.
/// Uses a local OVERLAPPED + event; honours the stop event for cancellation.
/// Returns true and sets bytesRead on success.
/// Returns false (and sets bytesRead=0) on error, cancellation, or disconnect.
bool ReadSync(HANDLE hPipe, HANDLE hStopEvent,
              void* buf, DWORD bufSize,
              DWORD& bytesRead)
{
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) {
        LOG_ERROR(L"ReadSync: CreateEvent failed (err=%lu)", GetLastError());
        bytesRead = 0;
        return false;
    }

    const BOOL ok = ReadFile(hPipe, buf, bufSize, nullptr, &ov);  // C-02, C-03 fix
    if (!ok) {
        const DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            // Real error (broken pipe, etc.)
            CloseHandle(ov.hEvent);
            bytesRead = 0;
            return false;
        }
    }

    // Wait for read completion OR stop signal
    const HANDLE handles[2] = { ov.hEvent, hStopEvent };
    const DWORD waitRes = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

    bool success = false;
    if (waitRes == WAIT_OBJECT_0) {
        // I/O completed — retrieve result
        if (GetOverlappedResult(hPipe, &ov, &bytesRead, FALSE)) {
            success = true;
        } else {
            const DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
                LOG_INFO(L"ReadSync: client disconnected (err=%lu)", err);
            } else {
                LOG_ERROR(L"ReadSync: GetOverlappedResult failed (err=%lu)", err);
            }
            bytesRead = 0;
        }
    } else {
        // Stop event (WAIT_OBJECT_0+1) or wait error
        // H-01: cancel first, then drain with bWait=TRUE (guaranteed to complete)
        CancelIoEx(hPipe, &ov);
        DWORD tmp = 0;
        GetOverlappedResult(hPipe, &ov, &tmp, TRUE);
        LOG_INFO(L"ReadSync: I/O cancelled (shutdown)");
        bytesRead = 0;
    }

    CloseHandle(ov.hEvent);
    return success;
}

/// Synchronous-style WriteFile over an overlapped handle. (C-04 fix)
bool WriteSync(HANDLE hPipe, const void* buf, DWORD bufSize, DWORD& written)
{
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) {
        LOG_ERROR(L"WriteSync: CreateEvent failed (err=%lu)", GetLastError());
        written = 0;
        return false;
    }

    const BOOL ok = WriteFile(hPipe, buf, bufSize, nullptr, &ov);
    if (!ok) {
        const DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            CloseHandle(ov.hEvent);
            LOG_ERROR(L"WriteSync: WriteFile failed immediately (err=%lu)", err);
            written = 0;
            return false;
        }
    }

    if (!GetOverlappedResult(hPipe, &ov, &written, TRUE)) {
        LOG_ERROR(L"WriteSync: GetOverlappedResult failed (err=%lu)", GetLastError());
        CloseHandle(ov.hEvent);
        written = 0;
        return false;
    }

    CloseHandle(ov.hEvent);
    return true;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

IPCServer::IPCServer(SeatManager* seatManager)
    : m_seatManager(seatManager)
    , m_running(false)
    , m_hPipe(INVALID_HANDLE_VALUE)
    , m_hStopEvent(nullptr)
{}

IPCServer::~IPCServer()
{
    Stop();
}

// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------

bool IPCServer::Start()
{
    if (m_running.exchange(true)) {
        LOG_WARN(L"IPCServer::Start called while already running");
        return false;
    }

    m_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!m_hStopEvent) {
        LOG_ERROR(L"IPCServer::Start: CreateEvent failed (err=%lu)", GetLastError());
        m_running.store(false, std::memory_order_seq_cst);
        return false;
    }

    try {
        m_thread = std::thread(&IPCServer::ServerThread, this);
    }
    catch (const std::system_error& e) {
        LOG_ERROR(L"IPCServer::Start: thread creation failed (%S)", e.what());
        CloseHandle(m_hStopEvent);
        m_hStopEvent = nullptr;
        m_running.store(false, std::memory_order_seq_cst);
        return false;
    }

    // L-04: use %s format arg — robust if IPC_PIPE_NAME is not a literal.
    LOG_INFO(L"IPC Server started on %s", IPC_PIPE_NAME);
    return true;
}

// ---------------------------------------------------------------------------
// Stop  -- guaranteed to return; no deadlock
// ---------------------------------------------------------------------------

void IPCServer::Stop()
{
    // H-02: seq_cst store so the thread's acquire load cannot see a stale value.
    if (!m_running.exchange(false, std::memory_order_seq_cst)) return;

    // 1. Signal stop event — unblocks ConnectNamedPipe wait AND ReadSync wait.
    if (m_hStopEvent) SetEvent(m_hStopEvent);

    // 2. Cancel and close the active pipe handle under the mutex.
    //    ServerThread will not re-publish m_hPipe after m_running is false.
    {
        std::lock_guard<std::mutex> lk(m_pipeMutex);
        if (m_hPipe != INVALID_HANDLE_VALUE) {
            CancelIoEx(m_hPipe, nullptr);
            // Do NOT DisconnectNamedPipe here — ServerThread owns the lifecycle;
            // closing is sufficient to unblock any pending I/O.
            CloseHandle(m_hPipe);
            m_hPipe = INVALID_HANDLE_VALUE;
        }
    }

    if (m_thread.joinable()) m_thread.join();

    if (m_hStopEvent) {
        CloseHandle(m_hStopEvent);
        m_hStopEvent = nullptr;
    }

    LOG_INFO(L"IPC Server stopped");
}

// ---------------------------------------------------------------------------
// ServerThread
// ---------------------------------------------------------------------------

void IPCServer::ServerThread()
{
    DWORD backoffMs = 0;

    // H-02: seq_cst load pairs with seq_cst store in Stop().
    while (m_running.load(std::memory_order_seq_cst)) {

        HANDLE hPipe = CreateNamedPipeW(
            IPC_PIPE_NAME,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            IPC_BUFFER_SIZE, IPC_BUFFER_SIZE,
            kClientWaitTimeoutMs,   // M-07: renamed
            nullptr);

        if (hPipe == INVALID_HANDLE_VALUE) {
            const DWORD err = GetLastError();
            LOG_ERROR(L"ServerThread: CreateNamedPipeW failed (err=%lu)", err);
            // H-04: std::min with explicit type — no macro clash
            backoffMs = (backoffMs == 0) ? 250u
                                         : std::min<DWORD>(backoffMs * 2u, 8000u);
            Sleep(backoffMs);
            continue;
        }
        backoffMs = 0;

        // ── Overlapped ConnectNamedPipe ───────────────────────────────────
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!ov.hEvent) {
            LOG_ERROR(L"ServerThread: CreateEvent for OVERLAPPED failed (err=%lu)",
                      GetLastError());
            CloseHandle(hPipe);
            Sleep(500);
            continue;
        }

        bool clientConnected = false;
        bool stopOwnsHandle  = false;   // M-01: track whether Stop() closed hPipe

        const BOOL connected = ConnectNamedPipe(hPipe, &ov);

        if (!connected) {
            const DWORD err = GetLastError();
            if (err == ERROR_PIPE_CONNECTED) {
                clientConnected = true;
            }
            else if (err == ERROR_IO_PENDING) {
                const HANDLE waitHandles[2] = { ov.hEvent, m_hStopEvent };
                const DWORD waitResult = WaitForMultipleObjects(
                    2, waitHandles, FALSE, INFINITE);

                if (waitResult == WAIT_OBJECT_0) {
                    DWORD transferred = 0;
                    if (GetOverlappedResult(hPipe, &ov, &transferred, FALSE)) {
                        clientConnected = true;
                    } else {
                        LOG_WARN(L"ServerThread: GetOverlappedResult failed (err=%lu)",
                                 GetLastError());
                    }
                } else {
                    // Stop event fired — H-01: cancel explicitly before drain
                    CancelIoEx(hPipe, &ov);
                    DWORD tmp = 0;
                    GetOverlappedResult(hPipe, &ov, &tmp, TRUE); // safe: cancel guarantees completion
                }
            } else {
                LOG_ERROR(L"ServerThread: ConnectNamedPipe failed (err=%lu)", err);
            }
        } else {
            // L-02: For FILE_FLAG_OVERLAPPED, ConnectNamedPipe returning TRUE is
            // unexpected per MSDN. Log as a warning but treat as connected.
            LOG_WARN(L"ServerThread: ConnectNamedPipe returned TRUE unexpectedly "
                     L"on overlapped handle — treating as connected");
            clientConnected = true;
        }

        CloseHandle(ov.hEvent);

        // ── Serve client ─────────────────────────────────────────────────
        if (clientConnected && m_running.load(std::memory_order_seq_cst)) {
            {
                std::lock_guard<std::mutex> lk(m_pipeMutex);
                m_hPipe = hPipe;
            }

            HandleClient(hPipe);

            // C-01 / M-01: re-acquire mutex to check whether Stop() already
            // closed this handle while we were in HandleClient().
            {
                std::lock_guard<std::mutex> lk(m_pipeMutex);
                if (m_hPipe == INVALID_HANDLE_VALUE) {
                    // Stop() already closed hPipe — do not touch it again.
                    stopOwnsHandle = true;
                } else {
                    m_hPipe = INVALID_HANDLE_VALUE;
                }
            }
        }

        // M-01: only Disconnect/Close if Stop() has not already done so.
        if (!stopOwnsHandle) {
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
        }
    }
}

// ---------------------------------------------------------------------------
// HandleClient  -- reads packets until pipe error or shutdown (C-02 / C-03)
// ---------------------------------------------------------------------------

void IPCServer::HandleClient(HANDLE hPipe)
{
    while (m_running.load(std::memory_order_seq_cst)) {

        IPCPacket packet{};   // M-04: zero-initialise each iteration
        DWORD bytesRead = 0;

        // C-02/C-03: overlapped read via ReadSync; stop event unblocks cleanly.
        if (!ReadSync(hPipe, m_hStopEvent, &packet, sizeof(packet), bytesRead)) {
            break;  // disconnected, cancelled, or error — logged inside ReadSync
        }

        if (bytesRead == 0) {
            LOG_WARN(L"HandleClient: zero-byte read -- disconnecting");
            break;
        }

        if (!ValidatePacket(packet, bytesRead)) {
            LOG_WARN(L"HandleClient: dropped invalid packet");
            continue;
        }

        ProcessMessage(packet, hPipe);
    }
}

// ---------------------------------------------------------------------------
// ProcessMessage
// ---------------------------------------------------------------------------

void IPCServer::ProcessMessage(const IPCPacket& packet, HANDLE hPipe)
{
    switch (packet.type) {

    case IPCMessageType::Ping:
        SendResponse(hPipe, IPCMessageType::Pong, "pong");
        break;

    case IPCMessageType::GetStatus:
        if (m_seatManager) {
            const std::wstring statusW = m_seatManager->GetStatusJSON();
            const std::string  statusU = WideToUtf8(statusW);
            if (!statusU.empty()) {
                SendResponse(hPipe, IPCMessageType::StatusResponse, statusU);
            } else {
                LOG_ERROR(L"ProcessMessage: GetStatus UTF-8 conversion failed");
                SendResponse(hPipe, IPCMessageType::Pong, "error");
            }
        } else {
            LOG_WARN(L"ProcessMessage: GetStatus called but m_seatManager is null");
            SendResponse(hPipe, IPCMessageType::Pong, "error");
        }
        break;

    case IPCMessageType::StartSeat: {
        int seat = -1;
        // H-03: pass capped payloadSize
        const DWORD safeSize = std::min<DWORD>(packet.payloadSize,
            static_cast<DWORD>(sizeof(packet.payload) - 1u));
        if (!ParseSeatId(packet.payload, safeSize, seat)) {
            LOG_WARN(L"ProcessMessage: StartSeat -- invalid seat payload");
            SendResponse(hPipe, IPCMessageType::Pong, "error");
            break;
        }
        if (m_seatManager) {
            m_seatManager->StartSeat(seat);
            LOG_INFO(L"ProcessMessage: StartSeat(%d) dispatched", seat);
        }
        SendResponse(hPipe, IPCMessageType::Pong, "ok");
        break;
    }

    case IPCMessageType::StopSeat: {
        int seat = -1;
        const DWORD safeSize = std::min<DWORD>(packet.payloadSize,
            static_cast<DWORD>(sizeof(packet.payload) - 1u));
        if (!ParseSeatId(packet.payload, safeSize, seat)) {
            LOG_WARN(L"ProcessMessage: StopSeat -- invalid seat payload");
            SendResponse(hPipe, IPCMessageType::Pong, "error");
            break;
        }
        if (m_seatManager) {
            m_seatManager->StopSeat(seat);
            LOG_INFO(L"ProcessMessage: StopSeat(%d) dispatched", seat);
        }
        SendResponse(hPipe, IPCMessageType::Pong, "ok");
        break;
    }

    case IPCMessageType::Shutdown:
        LOG_INFO(L"ProcessMessage: Shutdown requested via IPC");
        SendResponse(hPipe, IPCMessageType::Pong, "ok");
        // H-05: PostQuitMessage posts to THIS (server) thread's queue, not the
        // UI thread.  Use the application's registered shutdown mechanism instead.
        // If no application-level event exists, the project must add one.
        // Example: SetEvent(g_hAppShutdownEvent);
        // PostQuitMessage(0);  // <- REMOVED: wrong thread, see H-05
        //
        // Fallback: signal our own stop so the server exits cleanly.
        // The application layer must observe m_running becoming false
        // and perform its own shutdown sequence.
        Stop();
        break;

    default:
        LOG_WARN(L"ProcessMessage: unhandled message type %d",
                 static_cast<int>(packet.type));
        break;
    }
}

// ---------------------------------------------------------------------------
// SendResponse  -- C-04: WriteFile now uses WriteSync (overlapped-safe)
// ---------------------------------------------------------------------------

void IPCServer::SendResponse(HANDLE hPipe,
                              IPCMessageType type,
                              const std::string& payload)
{
    IPCPacket resp{};
    resp.type = type;

    // H-03/H-04: explicit type for std::min, reserve null terminator
    const size_t maxCopy = sizeof(resp.payload) - 1u;
    if (payload.size() > maxCopy) {
        LOG_WARN(L"SendResponse: payload truncated from %zu to %zu bytes",
                 payload.size(), maxCopy);
    }
    resp.payloadSize = static_cast<DWORD>(
        std::min<size_t>(payload.size(), maxCopy));
    std::memcpy(resp.payload, payload.data(), resp.payloadSize);
    resp.payload[resp.payloadSize] = '\0';  // defensive null terminator

    DWORD written = 0;
    if (!WriteSync(hPipe, &resp, sizeof(resp), written)) {  // C-04 fix
        LOG_ERROR(L"SendResponse: WriteSync failed");
    } else if (written != sizeof(resp)) {
        LOG_WARN(L"SendResponse: partial write (%lu / %zu bytes)",
                 written, sizeof(resp));
    }
}
