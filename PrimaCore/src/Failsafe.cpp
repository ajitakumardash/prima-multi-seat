// ============================================================
// Prima Multi Seat - Failsafe Implementation
// Ctrl+Alt+P emergency recovery
// ============================================================

#include "../include/Failsafe.h"
#include "../include/SeatManager.h"

Failsafe::Failsafe(SeatManager* seatManager)
    : m_seatManager(seatManager)
    , m_triggered(false)
    , m_hWnd(nullptr)
{}

Failsafe::~Failsafe() {
    if (m_hWnd) UnregisterHotkey(m_hWnd);
}

bool Failsafe::RegisterHotkey(HWND hWnd) {
    m_hWnd = hWnd;
    if (!RegisterHotKey(hWnd, FAILSAFE_HOTKEY_ID, FAILSAFE_MOD, FAILSAFE_VK)) {
        LOG_WARN(L"Could not register Ctrl+Alt+P hotkey (may be in use)");
        return false;
    }
    LOG_INFO(L"Failsafe hotkey Ctrl+Alt+P registered");
    return true;
}

void Failsafe::UnregisterHotkey(HWND hWnd) {
    UnregisterHotKey(hWnd, FAILSAFE_HOTKEY_ID);
}

void Failsafe::Trigger() {
    if (m_triggered.exchange(true)) return; // already triggered
    LOG_WARN(L"FAILSAFE TRIGGERED - emergency recovery");
    RestoreSystem();
    ShowRecoveryDialog();
}

void Failsafe::Reset() {
    m_triggered = false;
    LOG_INFO(L"Failsafe reset");
}

void Failsafe::RestoreSystem() {
    if (!m_seatManager) return;

    // Stop all seats
    for (int i = 0; i < MAX_SEATS; i++)
        m_seatManager->StopSeat(i);

    // Restore system cursor
    m_seatManager->GetCursorEngine()->ShowSystemCursor();

    // Enable passthrough (all input goes to Windows normally)
    LOG_INFO(L"System restored to normal state");
}

void Failsafe::ShowRecoveryDialog() {
    int result = MessageBoxW(nullptr,
        L"Prima Multi Seat Failsafe Recovery\n\n"
        L"All seat isolation has been disabled.\n"
        L"The system cursor has been restored.\n\n"
        L"Options:\n"
        L"  [Yes]  Restart seat isolation\n"
        L"  [No]   Keep in safe mode\n"
        L"  [Cancel] Exit Prima Multi Seat",
        L"Prima Multi Seat - Failsafe Recovery",
        MB_YESNOCANCEL | MB_ICONWARNING | MB_TOPMOST);

    if (result == IDYES) {
        Reset();
        if (m_seatManager) {
            m_seatManager->StartSeat(SEAT_1);
            m_seatManager->StartSeat(SEAT_2);
        }
    } else if (result == IDCANCEL) {
        PostQuitMessage(0);
    }
    // IDNO = stay in safe mode
}