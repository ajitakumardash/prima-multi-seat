// ============================================================
// Prima Multi Seat - Window Isolator Implementation
// ============================================================

#include "../include/WindowIsolator.h"

WindowIsolator::WindowIsolator()
    : m_hMoveHook(nullptr)
    , m_hForegroundHook(nullptr)
{
    for (int i = 0; i < MAX_SEATS; i++)
        ZeroMemory(&m_seatBounds[i], sizeof(RECT));
}

WindowIsolator::~WindowIsolator() { Shutdown(); }

bool WindowIsolator::Initialize() {
    // Hook for window move/resize events
    m_hMoveHook = SetWinEventHook(
        EVENT_SYSTEM_MOVESIZEEND, EVENT_SYSTEM_MOVESIZEEND,
        nullptr, WinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    // Hook for foreground window change
    m_hForegroundHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, WinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    LOG_INFO(L"WindowIsolator initialized");
    return true;
}

void WindowIsolator::Shutdown() {
    if (m_hMoveHook)       { UnhookWinEvent(m_hMoveHook);       m_hMoveHook = nullptr;       }
    if (m_hForegroundHook) { UnhookWinEvent(m_hForegroundHook); m_hForegroundHook = nullptr; }
    LOG_INFO(L"WindowIsolator shut down");
}

void WindowIsolator::SetSeatBounds(int seatIndex, const RECT& bounds) {
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return;
    m_seatBounds[seatIndex] = bounds;
}

void WindowIsolator::AssignWindowToSeat(HWND hWnd, int seatIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);
    WindowRecord rec;
    rec.hWnd           = hWnd;
    rec.seatIndex      = seatIndex;
    rec.locked         = true;
    rec.lastValidRect  = GetWindowRectSafe(hWnd);
    GetWindowThreadProcessId(hWnd, &rec.processId);
    m_windows[hWnd] = rec;
}

void WindowIsolator::RemoveWindowFromSeat(HWND hWnd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_windows.erase(hWnd);
}

int WindowIsolator::GetSeatForWindow(HWND hWnd) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_windows.find(hWnd);
    return (it != m_windows.end()) ? it->second.seatIndex : SEAT_NONE;
}

void WindowIsolator::LockWindowToSeat(HWND hWnd, bool locked) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_windows.find(hWnd);
    if (it != m_windows.end()) it->second.locked = locked;
}

bool WindowIsolator::IsWindowLocked(HWND hWnd) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_windows.find(hWnd);
    return (it != m_windows.end()) && it->second.locked;
}

void WindowIsolator::OnWindowMoveResize(HWND hWnd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_windows.find(hWnd);
    if (it == m_windows.end()) return;
    EnforceWindow(it->second);
}

void WindowIsolator::EnforceWindow(WindowRecord& rec) {
    if (!rec.locked || rec.seatIndex < 0 || rec.seatIndex >= MAX_SEATS) return;
    if (!IsWindow(rec.hWnd)) {
        m_windows.erase(rec.hWnd);
        return;
    }

    RECT wr = GetWindowRectSafe(rec.hWnd);
    const RECT& seatBounds = m_seatBounds[rec.seatIndex];

    // Check if window center is outside seat bounds
    LONG cx = (wr.left + wr.right) / 2;
    LONG cy = (wr.top  + wr.bottom) / 2;

    if (cx < seatBounds.left || cx >= seatBounds.right ||
        cy < seatBounds.top  || cy >= seatBounds.bottom) {
        // Move window back to last valid position
        MoveWindow(rec.hWnd,
            rec.lastValidRect.left, rec.lastValidRect.top,
            rec.lastValidRect.right - rec.lastValidRect.left,
            rec.lastValidRect.bottom - rec.lastValidRect.top,
            TRUE);
        LOG_INFO(L"Window restored to seat monitor");
    } else {
        rec.lastValidRect = wr;
    }
}

void WindowIsolator::EnforceAllWindows() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [hWnd, rec] : m_windows) EnforceWindow(rec);
}

HWND WindowIsolator::GetSeatForegroundWindow(int seatIndex) const {
    return GetForegroundWindow(); // simplified
}

void CALLBACK WindowIsolator::WinEventProc(
    HWINEVENTHOOK, DWORD event, HWND hwnd,
    LONG, LONG, DWORD, DWORD)
{
    // NOTE: In a full implementation, we'd keep a static pointer
    // to the WindowIsolator singleton and call OnWindowMoveResize
    // For now this is a placeholder
    (void)event;
    (void)hwnd;
}

RECT WindowIsolator::GetWindowRectSafe(HWND hWnd) const {
    RECT r = {};
    if (IsWindow(hWnd)) GetWindowRect(hWnd, &r);
    return r;
}