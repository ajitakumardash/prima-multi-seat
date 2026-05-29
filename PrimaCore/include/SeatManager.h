#pragma once
// ============================================================
// Prima Multi Seat - Seat Manager
// Central coordinator: manages seat lifecycle, device
// assignments, and inter-module communication.
// ============================================================

#include "Common.h"
#include "DeviceManager.h"
#include "CursorEngine.h"
#include "KeyboardRouter.h"
#include "MouseRouter.h"
#include "WindowIsolator.h"
#include "AudioRouter.h"
#include "ConfigManager.h"

class SeatManager {
public:
    SeatManager();
    ~SeatManager();

    // ─── Lifecycle ───────────────────────────────────────────
    bool Initialize(HWND messageWindow);
    void Shutdown();
    void Update();  // Called from main message loop

    // ─── Seat Control ────────────────────────────────────────
    bool StartSeat(int seatIndex);
    bool StopSeat(int seatIndex);
    bool IsSeatActive(int seatIndex) const;
    SeatState GetSeatState(int seatIndex) const;

    // ─── Device Assignment ───────────────────────────────────
    bool AssignKeyboardToSeat(HANDLE hDevice, int seatIndex);
    bool AssignMouseToSeat(HANDLE hDevice, int seatIndex);
    bool AssignMonitorToSeat(int monitorIndex, int seatIndex);
    bool AssignAudioToSeat(const std::wstring& deviceId, int seatIndex);

    // ─── Raw Input Dispatch ──────────────────────────────────
    void OnRawInput(HRAWINPUT hRawInput);

    // ─── Config Load/Save ────────────────────────────────────
    bool LoadConfig(const std::wstring& configPath);
    bool SaveConfig(const std::wstring& configPath);
    void ApplyDefaultConfig();

    // ─── Subsystem Access ────────────────────────────────────
    DeviceManager*  GetDeviceManager()  { return m_deviceManager.get(); }
    CursorEngine*   GetCursorEngine()   { return m_cursorEngine.get();  }
    AudioRouter*    GetAudioRouter()    { return m_audioRouter.get();   }
    WindowIsolator* GetWindowIsolator() { return m_windowIsolator.get();}

    // ─── Status ──────────────────────────────────────────────
    std::wstring GetStatusJSON() const;

private:
    std::unique_ptr<DeviceManager>  m_deviceManager;
    std::unique_ptr<CursorEngine>   m_cursorEngine;
    std::unique_ptr<KeyboardRouter> m_keyboardRouter;
    std::unique_ptr<MouseRouter>    m_mouseRouter;
    std::unique_ptr<WindowIsolator> m_windowIsolator;
    std::unique_ptr<AudioRouter>    m_audioRouter;
    std::unique_ptr<ConfigManager>  m_configManager;

    SeatConfig  m_seats[MAX_SEATS];
    HWND        m_messageWindow;
    bool        m_initialized;
    mutable std::mutex m_mutex;

    void OnDeviceChange();
    void UpdateSeatCursors();
};