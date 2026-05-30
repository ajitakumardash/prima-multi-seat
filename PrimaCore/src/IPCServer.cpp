// ============================================================
// Prima Multi Seat - IPC Server Implementation  [PRODUCTION]
// Named pipe server for UI <-> Core communication
// Hardened: thread-safe, overlapped I/O, robust validation
// ============================================================

#include "../include/IPCServer.h"
#include "../include/SeatManager.h"

#include <atomic>
#include <stdexcept>
#include <cassert>
#include <charconv>      // std::from_chars
#include <string_view>

// ---------------------------------------------------------------------------
// Internal constants
// ---------------------------------------------------------------------------
static constexpr DWORD kConnectTimeoutMs = 500;   // poll interval for shutdown
static constexpr DWORD kMaxPayloadDisplay = 64;    // log truncation limit
static constexpr int   kMaxSeatId        = 9999;  // sanity cap for seat IDs
static constexpr DWORD kPipeOpenTimeout  = 5000;  // ms for CreateNamedPipe retry

// ---------------------------------------------------------------------------
// UTF-8 conversion helpers (no raw sz-1 arithmetic)
// ---------------------------------------------------------------------------
namespace {

/// Wide -> UTF-8.  Returns empty string on failure.
std::string WideToUtf8(const std::wstring& wide) noexcept
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

/// UTF-8 -> Wide.  Returns empty string on failure.
std::wstring Utf8ToWide(std::string_view utf8) noexcept
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

/// Parse a seat ID from a null-terminated payload buffer.
/// Returns false if the payload is empty, non-numeric, or out of range.
bool ParseSeatId(const char* payload, size_t maxLen, int& outSeat) noexcept
{
    if (!payload || maxLen == 0 || payload[0] == '\0') {
        LOG_WARN(L"ParseSeatId: empty payload");
        return false;
    }

    const size_t len = strnlen(payload, maxLen);
    if (len == 0) {
        LOG_WARN(L"ParseSeatId: zero-length content");
        return false;
    }

    int value = 0;
    const auto [ptr, ec] = std::from_chars(payload, payload + len, value);

    if (ec != std::errc{}) {
        LOG_WARN(L"ParseSeatId: non-numeric payload (%.32S)", payload);
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
bool ValidatePacket(const IPCPacket& pkt, DWORD bytesRead) noexcept
{
    if (bytesRead != sizeof(IPCPacket)) {
        LOG_WARN(L"ValidatePacket: unexpected size (got %lu, want %zu)",
                 bytesRead, sizeof(IPCPacket));
        return false;
    }
    if (pkt.payloadSize > sizeof(pkt.payload)) {
        LOG_WARN(L"ValidatePacket: payloadSize %lu exceeds buffer %zu",
                 pkt.payloadSize, sizeof(pkt.payload));
        return false;
    }
    const auto typeVal = static_cast<std::underlying_type_t<IPCMessageType>>(pkt.type);
    const auto minType = static_cast<std::underlying_type_t<IPCMessageType>>(IPCMessageType::Ping);
    const auto maxType = static_cast<std::underlying_type_t<IPCMessageType>>(IPCMessageType::Shutdown);
    if (typeVal < minType || typeVal > maxType) {
        LOG_WARN(L"ValidatePacket: unknown message type value %d", typeVal);
        return false;
    }
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

    // Manual-reset event; signalled during Stop() to unblock ConnectNamedPipe
    m_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!m_hStopEvent) {
        LOG_ERROR(L"IPCServer::Start: CreateEvent failed (err=%lu)", GetLastError());
        m_running.store(false);
        return false;
    }

    try {
        m_thread = std::thread(&IPCServer::ServerThread, this);
    }
    catch (const std::system_error& e) {
        LOG_ERROR(L"IPCServer::Start: thread creation failed (%S)", e.what());
        CloseHandle(m_hStopEvent);
        m_hStopEvent = nullptr;
        m_running.store(false);
        return false;
    }

    LOG_INFO(L"IPC Server started on " IPC_PIPE_NAME);
    return true;
}

// ---------------------------------------------------------------------------
// Stop  -- guaranteed to return; no deadlock
// ---------------------------------------------------------------------------

void IPCServer::Stop()
{
    if (!m_running.exchange(false)) return;  // already stopped

    // 1. Signal the stop event to wake ConnectNamedPipe (overlapped path)
    if (m_hStopEvent) SetEvent(m_hStopEvent);

    // 2. Close the pipe under the mutex so HandleClient's ReadFile unblocks
    {
        std::lock_guard<std::mutex> lk(m_pipeMutex);
        if (m_hPipe != INVALID_HANDLE_VALUE) {
            CancelIoEx(m_hPipe, nullptr);
            DisconnectNamedPipe(m_hPipe);
            CloseHandle(m_hPipe);
            m_hPipe = INVALID_HANDLE_VALUE;
        }
    }

    // 3. Join worker thread
    if (m_thread.joinable()) m_thread.join();

    // 4. Clean up event
    if (m_hStopEvent) {
        CloseHandle(m_hStopEvent);
        m_hStopEvent = nullptr;
    }

    LOG_INFO(L"IPC Server stopped");
}

// ---------------------------------------------------------------------------
// ServerThread  -- overlapped ConnectNamedPipe so Stop() is never blocked
// ---------------------------------------------------------------------------

void IPCServer::ServerThread()
{
    DWORD backoffMs = 0;    // exponential back-off on repeated API failures

    while (m_running.load(std::memory_order_acquire)) {

        // -- Create pipe --------------------------------------------------
        HANDLE hPipe = CreateNamedPipeW(
            IPC_PIPE_NAME,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            IPC_BUFFER_SIZE, IPC_BUFFER_SIZE,
            kPipeOpenTimeout, nullptr);

        if (hPipe == INVALID_HANDLE_VALUE) {
            const DWORD err = GetLastError();
            LOG_ERROR(L"ServerThread: CreateNamedPipeW failed (err=%lu)", err);
            backoffMs = (backoffMs == 0) ? 250 : min(backoffMs * 2, 8000UL);
            Sleep(backoffMs);
            continue;
        }
        backoffMs = 0;

        // -- Overlapped ConnectNamedPipe -----------------------------------
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
        BOOL connected       = ConnectNamedPipe(hPipe, &ov);

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
                    // Stop event fired or wait error
                    CancelIoEx(hPipe, &ov);
                    DWORD tmp = 0;
                    GetOverlappedResult(hPipe, &ov, &tmp, TRUE); // drain
                }
            } else {
                LOG_ERROR(L"ServerThread: ConnectNamedPipe failed (err=%lu)", err);
            }
        } else {
            clientConnected = true;
        }

        CloseHandle(ov.hEvent);

        // -- Serve client -------------------------------------------------
        if (clientConnected && m_running.load(std::memory_order_acquire)) {
            {
                std::lock_guard<std::mutex> lk(m_pipeMutex);
                m_hPipe = hPipe;
            }

            HandleClient(hPipe);

            {
                std::lock_guard<std::mutex> lk(m_pipeMutex);
                m_hPipe = INVALID_HANDLE_VALUE;
            }
        }

        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
}

// ---------------------------------------------------------------------------
// HandleClient  -- reads packets until pipe error or shutdown
// ---------------------------------------------------------------------------

void IPCServer::HandleClient(HANDLE hPipe)
{
    IPCPacket packet{};  // zero-initialised

    while (m_running.load(std::memory_order_acquire)) {
        DWORD bytesRead = 0;
        const BOOL ok = ReadFile(hPipe, &packet, sizeof(packet), &bytesRead, nullptr);

        if (!ok) {
            const DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
                LOG_INFO(L"HandleClient: client disconnected (err=%lu)", err);
            } else if (err == ERROR_OPERATION_ABORTED) {
                LOG_INFO(L"HandleClient: I/O cancelled (shutdown)");
            } else {
                LOG_ERROR(L"HandleClient: ReadFile failed (err=%lu)", err);
            }
            break;
        }

        if (bytesRead == 0) {
            LOG_WARN(L"HandleClient: zero-byte read -- disconnecting");
            break;
        }

        if (!ValidatePacket(packet, bytesRead)) {
            LOG_WARN(L"HandleClient: dropped invalid packet");
            continue;  // allow client to recover
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

    // -- Ping -------------------------------------------------------------
    case IPCMessageType::Ping:
        SendResponse(hPipe, IPCMessageType::Pong, "pong");
        break;

    // -- GetStatus --------------------------------------------------------
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

    // -- StartSeat --------------------------------------------------------
    case IPCMessageType::StartSeat: {
        int seat = -1;
        if (!ParseSeatId(packet.payload, packet.payloadSize, seat)) {
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

    // -- StopSeat ---------------------------------------------------------
    case IPCMessageType::StopSeat: {
        int seat = -1;
        if (!ParseSeatId(packet.payload, packet.payloadSize, seat)) {
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

    // -- Shutdown ---------------------------------------------------------
    case IPCMessageType::Shutdown:
        LOG_INFO(L"ProcessMessage: Shutdown requested via IPC");
        SendResponse(hPipe, IPCMessageType::Pong, "ok");  // ACK before quit
        PostQuitMessage(0);
        break;

    // -- Unknown ----------------------------------------------------------
    default:
        LOG_WARN(L"ProcessMessage: unhandled message type %d",
                 static_cast<int>(packet.type));
        break;
    }
}

// ---------------------------------------------------------------------------
// SendResponse  -- validated write; logs failure instead of silently dropping
// ---------------------------------------------------------------------------

void IPCServer::SendResponse(HANDLE hPipe,
                              IPCMessageType type,
                              const std::string& payload)
{
    IPCPacket resp{};
    resp.type = type;

    const size_t maxCopy = sizeof(resp.payload) - 1;   // reserve null terminator
    if (payload.size() > maxCopy) {
        LOG_WARN(L"SendResponse: payload truncated from %zu to %zu bytes",
                 payload.size(), maxCopy);
    }
    resp.payloadSize = static_cast<DWORD>(min(payload.size(), maxCopy));
    std::memcpy(resp.payload, payload.data(), resp.payloadSize);
    resp.payload[resp.payloadSize] = '\0';  // defensive null terminator

    DWORD written = 0;
    const BOOL ok = WriteFile(hPipe, &resp, sizeof(resp), &written, nullptr);
    if (!ok) {
        LOG_ERROR(L"SendResponse: WriteFile failed (err=%lu)", GetLastError());
    } else if (written != sizeof(resp)) {
        LOG_WARN(L"SendResponse: partial write (%lu / %zu bytes)",
                 written, sizeof(resp));
    }
}
