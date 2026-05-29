#pragma once
// ============================================================
// Prima Multi Seat - Device Manager
// Enumerates and tracks keyboards, mice, and monitors using
// Raw Input API and EnumDisplayMonitors.
// ============================================================

#include "Common.h"
#include <hidsdi.h>
#include <setupapi.h>

class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    // ─── Initialization ──────────────────────────────────────
    bool Initialize(HWND messageWindow);
    void Shutdown();

    // ─── Device Enumeration ──────────────────────────────────
    void EnumerateDevices();
    void EnumerateMonitors();

    // ─── Raw Input Registration ───────────────────────────────
    bool RegisterRawInput(HWND hWnd);

    // ─── Lookup ──────────────────────────────────────────────
    DeviceInfo* FindDevice(HANDLE hDevice);
    MonitorInfo* FindMonitor(int index);
    MonitorInfo* MonitorFromPoint(POINT pt);

    // ─── Collections ─────────────────────────────────────────
    const std::vector<DeviceInfo>&  GetKeyboards()  const { return m_keyboards; }
    const std::vector<DeviceInfo>&  GetMice()       const { return m_mice;      }
    const std::vector<MonitorInfo>& GetMonitors()   const { return m_monitors;  }

    // ─── Seat Assignment ─────────────────────────────────────
    bool AssignDeviceToSeat(HANDLE hDevice, int seatIndex);
    bool AssignMonitorToSeat(int monitorIndex, int seatIndex);

    // ─── Device Change Notification ──────────────────────────
    void OnDeviceChange(WPARAM wParam, LPARAM lParam);

    // ─── Callbacks ───────────────────────────────────────────
    using DeviceChangeCallback = std::function<void()>;
    void SetDeviceChangeCallback(DeviceChangeCallback cb) { m_onDeviceChange = cb; }

private:
    std::vector<DeviceInfo>  m_keyboards;
    std::vector<DeviceInfo>  m_mice;
    std::vector<MonitorInfo> m_monitors;
    HWND                     m_messageWindow;
    HDEVNOTIFY               m_hDevNotify;
    DeviceChangeCallback     m_onDeviceChange;
    mutable std::mutex       m_deviceMutex;

    // ─── Helpers ─────────────────────────────────────────────
    std::wstring GetDeviceFriendlyName(HANDLE hDevice);
    std::wstring GetDevicePath(HANDLE hDevice);

    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC hDC,
                                          LPRECT lpRect, LPARAM lParam);
};