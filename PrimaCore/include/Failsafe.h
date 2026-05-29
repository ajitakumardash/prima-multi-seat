#pragma once
// ============================================================
// Prima Multi Seat - Failsafe
// Emergency hotkey (Ctrl+Alt+P) handler:
//   - Disables input routing
//   - Restores system cursor
//   - Shows all windows on primary monitor
//   - Re-enables all audio
// ============================================================

#include "Common.h"

class SeatManager;

class Failsafe {
public:
    explicit Failsafe(SeatManager* seatManager);
    ~Failsafe();

    // ─── Registration ────────────────────────────────────────
    bool RegisterHotkey(HWND hWnd);
    void UnregisterHotkey(HWND hWnd);

    // ─── Trigger ─────────────────────────────────────────────
    void Trigger();

    // ─── State ───────────────────────────────────────────────
    bool IsTriggered() const { return m_triggered; }
    void Reset();

private:
    SeatManager*        m_seatManager;
    std::atomic<bool>   m_triggered;
    HWND                m_hWnd;

    void RestoreSystem();
    void ShowRecoveryDialog();
};