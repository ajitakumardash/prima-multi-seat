#pragma once
// ============================================================
// Prima Multi Seat - Mouse Router
// Routes Raw Input mouse events to seat-specific cursor and
// injects synthetic mouse messages to the window under cursor.
// ============================================================

#include "Common.h"
#include "CursorEngine.h"

class MouseRouter {
public:
    explicit MouseRouter(CursorEngine* cursorEngine);
    ~MouseRouter();

    bool Initialize();
    void Shutdown();

    // ─── Raw Input Processing ────────────────────────────────
    void ProcessRawInput(HRAWINPUT hRawInput);

    // ─── Device→Seat Mapping ────────────────────────────────
    void MapDeviceToSeat(HANDLE hDevice, int seatIndex);
    void UnmapDevice(HANDLE hDevice);
    int  GetSeatForDevice(HANDLE hDevice) const;

    // ─── Sensitivity ─────────────────────────────────────────
    void SetSensitivity(float sensitivity) { m_sensitivity = sensitivity; }

    // ─── Passthrough ─────────────────────────────────────────
    void SetPassthroughMode(bool enabled);

private:
    CursorEngine*           m_cursorEngine;
    std::map<HANDLE, int>   m_deviceSeatMap;
    std::atomic<bool>       m_passthroughMode;
    float                   m_sensitivity;
    mutable std::mutex      m_mutex;

    // ─── Click Injection ─────────────────────────────────────
    void InjectMouseClick(int seatIndex, UINT message, WPARAM wParam);
    void InjectMouseMove(int seatIndex, POINT screenPt);
    HWND WindowUnderCursor(POINT screenPt);
};