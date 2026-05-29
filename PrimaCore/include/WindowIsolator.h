#pragma once
// ============================================================
// Prima Multi Seat - Window Isolator
// Monitors foreground window changes and moves windows that
// belong to a seat back to the assigned monitor if they stray.
// Uses SetWinEventHook for real-time window tracking.
// ============================================================

#include "Common.h"

struct WindowRecord {
    HWND    hWnd;
    int     seatIndex;
    RECT    lastValidRect;
    DWORD   processId;
    bool    locked;
};

class WindowIsolator {
public:
    WindowIsolator();
    ~WindowIsolator();

    bool Initialize();
    void Shutdown();

    // ─── Seat Monitor Bounds ─────────────────────────────────
    void SetSeatBounds(int seatIndex, const RECT& bounds);

    // ─── Window→Seat Assignment ──────────────────────────────
    void AssignWindowToSeat(HWND hWnd, int seatIndex);
    void RemoveWindowFromSeat(HWND hWnd);
    int  GetSeatForWindow(HWND hWnd) const;

    // ─── Lock / Unlock ───────────────────────────────────────
    void LockWindowToSeat(HWND hWnd, bool locked);
    bool IsWindowLocked(HWND hWnd) const;

    // ─── Event Driven ────────────────────────────────────────
    void OnWindowMoveResize(HWND hWnd);
    void OnForegroundChange(HWND hWnd);
    void EnforceAllWindows();

    // ─── Foreground Tracking ─────────────────────────────────
    HWND GetSeatForegroundWindow(int seatIndex) const;

private:
    std::map<HWND, WindowRecord> m_windows;
    RECT                         m_seatBounds[MAX_SEATS];
    HWINEVENTHOOK                m_hMoveHook;
    HWINEVENTHOOK                m_hForegroundHook;
    mutable std::mutex           m_mutex;

    static void CALLBACK WinEventProc(
        HWINEVENTHOOK hWinEventHook, DWORD event,
        HWND hwnd, LONG idObject, LONG idChild,
        DWORD dwEventThread, DWORD dwmsEventTime);

    void EnforceWindow(WindowRecord& rec);
    bool IsWindowOnSeatMonitor(HWND hWnd, int seatIndex) const;
    RECT GetWindowRectSafe(HWND hWnd) const;
};