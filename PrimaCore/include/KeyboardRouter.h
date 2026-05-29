#pragma once
// ============================================================
// Prima Multi Seat - Keyboard Router
// Routes Raw Input keyboard events to the focused window
// of the seat that owns the physical keyboard device.
// ============================================================

#include "Common.h"

class KeyboardRouter {
public:
    KeyboardRouter();
    ~KeyboardRouter();

    bool Initialize();
    void Shutdown();

    // ─── Raw Input Processing ────────────────────────────────
    // Call this from WM_INPUT handler with the raw input handle
    void ProcessRawInput(HRAWINPUT hRawInput);

    // ─── Seat Window Tracking ────────────────────────────────
    // Register the foreground window for a seat
    void SetSeatFocusWindow(int seatIndex, HWND hWnd);
    HWND GetSeatFocusWindow(int seatIndex) const;

    // ─── Device→Seat Mapping ────────────────────────────────
    void MapDeviceToSeat(HANDLE hDevice, int seatIndex);
    void UnmapDevice(HANDLE hDevice);
    int  GetSeatForDevice(HANDLE hDevice) const;

    // ─── Passthrough mode ────────────────────────────────────
    // When active, all input goes to seat 1 (safe mode)
    void SetPassthroughMode(bool enabled);
    bool IsPassthroughMode() const { return m_passthroughMode; }

private:
    std::map<HANDLE, int>   m_deviceSeatMap;
    HWND                    m_seatWindows[MAX_SEATS];
    std::atomic<bool>       m_passthroughMode;
    mutable std::mutex      m_mutex;

    // ─── Key Injection ───────────────────────────────────────
    // Injects a keystroke to a specific window using PostMessage.
    // Uses WM_KEYDOWN / WM_KEYUP / WM_CHAR for broad compatibility.
    void InjectKeyToWindow(HWND hWnd, const RAWKEYBOARD& rk);
    UINT MapVirtualKeyToChar(UINT vk, UINT scanCode, bool shift);
};