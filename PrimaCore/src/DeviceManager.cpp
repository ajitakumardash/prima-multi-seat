// ============================================================
// Prima Multi Seat - Device Manager Implementation
// ============================================================

#include "../include/DeviceManager.h"
#include <hidsdi.h>
#include <dbt.h>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

DeviceManager::DeviceManager()
    : m_messageWindow(nullptr)
    , m_hDevNotify(nullptr)
{}

DeviceManager::~DeviceManager() { Shutdown(); }

bool DeviceManager::Initialize(HWND messageWindow) {
    m_messageWindow = messageWindow;

    // Register for Raw Input devices (keyboards + mice)
    if (!RegisterRawInput(messageWindow)) {
        LOG_ERROR(L"Failed to register Raw Input");
        return false;
    }

    // Register for device change notifications
    DEV_BROADCAST_DEVICEINTERFACE filter = {};
    filter.dbcc_size       = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    // HID devices GUID
    filter.dbcc_classguid  = {0x4D1E55B2,0xF16F,0x11CF,
                               {0x88,0xCB,0x00,0x11,0x11,0x00,0x00,0x30}};
    m_hDevNotify = RegisterDeviceNotificationW(
        messageWindow,
        &filter,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );

    EnumerateDevices();
    EnumerateMonitors();

    LOG_INFO(L"DeviceManager initialized");
    return true;
}

void DeviceManager::Shutdown() {
    if (m_hDevNotify) {
        UnregisterDeviceNotification(m_hDevNotify);
        m_hDevNotify = nullptr;
    }
    LOG_INFO(L"DeviceManager shut down");
}

bool DeviceManager::RegisterRawInput(HWND hWnd) {
    // Register keyboard and mouse for raw input
    RAWINPUTDEVICE rid[2] = {};

    // Keyboard - Usage Page: Generic Desktop, Usage: Keyboard
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage     = 0x06;
    rid[0].dwFlags     = RIDEV_INPUTSINK | RIDEV_NOLEGACY;
    rid[0].hwndTarget  = hWnd;

    // Mouse - Usage Page: Generic Desktop, Usage: Mouse
    rid[1].usUsagePage = 0x01;
    rid[1].usUsage     = 0x02;
    rid[1].dwFlags     = RIDEV_INPUTSINK | RIDEV_NOLEGACY;
    rid[1].hwndTarget  = hWnd;

    if (!RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE))) {
        DWORD err = GetLastError();
        LOG_ERROR(L"RegisterRawInputDevices failed, error: " + std::to_wstring(err));
        return false;
    }

    LOG_INFO(L"Raw Input registered for keyboard and mouse");
    return true;
}

void DeviceManager::EnumerateDevices() {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    m_keyboards.clear();
    m_mice.clear();

    UINT numDevices = 0;
    GetRawInputDeviceList(nullptr, &numDevices, sizeof(RAWINPUTDEVICELIST));
    if (numDevices == 0) return;

    std::vector<RAWINPUTDEVICELIST> deviceList(numDevices);
    GetRawInputDeviceList(deviceList.data(), &numDevices, sizeof(RAWINPUTDEVICELIST));

    for (const auto& rawDevice : deviceList) {
        DeviceInfo info;
        info.hDevice     = rawDevice.hDevice;
        info.isConnected = true;
        info.assignedSeat = SEAT_NONE;
        info.devicePath  = GetDevicePath(rawDevice.hDevice);
        info.friendlyName = GetDeviceFriendlyName(rawDevice.hDevice);

        if (rawDevice.dwType == RIM_TYPEKEYBOARD) {
            info.type = DeviceType::Keyboard;
            m_keyboards.push_back(info);
            LOG_INFO(L"Keyboard found: " + info.friendlyName);
        }
        else if (rawDevice.dwType == RIM_TYPEMOUSE) {
            info.type = DeviceType::Mouse;
            m_mice.push_back(info);
            LOG_INFO(L"Mouse found: " + info.friendlyName);
        }
    }

    LOG_INFO(L"Enumerated " + std::to_wstring(m_keyboards.size()) +
             L" keyboards, " + std::to_wstring(m_mice.size()) + L" mice");
}

void DeviceManager::EnumerateMonitors() {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    m_monitors.clear();
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc,
                         reinterpret_cast<LPARAM>(this));
    LOG_INFO(L"Enumerated " + std::to_wstring(m_monitors.size()) + L" monitors");
}

BOOL CALLBACK DeviceManager::MonitorEnumProc(
    HMONITOR hMon, HDC, LPRECT, LPARAM lParam)
{
    auto* self = reinterpret_cast<DeviceManager*>(lParam);
    MONITORINFOEXW mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMon, &mi)) {
        MonitorInfo info;
        info.hMonitor    = hMon;
        info.bounds      = mi.rcMonitor;
        info.workArea    = mi.rcWork;
        info.deviceName  = mi.szDevice;
        info.isPrimary   = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
        info.index       = (int)self->m_monitors.size();
        info.assignedSeat = SEAT_NONE;
        self->m_monitors.push_back(info);
    }
    return TRUE;
}

DeviceInfo* DeviceManager::FindDevice(HANDLE hDevice) {
    for (auto& d : m_keyboards)
        if (d.hDevice == hDevice) return &d;
    for (auto& d : m_mice)
        if (d.hDevice == hDevice) return &d;
    return nullptr;
}

MonitorInfo* DeviceManager::FindMonitor(int index) {
    if (index >= 0 && index < (int)m_monitors.size())
        return &m_monitors[index];
    return nullptr;
}

bool DeviceManager::AssignDeviceToSeat(HANDLE hDevice, int seatIndex) {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    auto* dev = FindDevice(hDevice);
    if (!dev) return false;
    dev->assignedSeat = seatIndex;
    return true;
}

bool DeviceManager::AssignMonitorToSeat(int monitorIndex, int seatIndex) {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    auto* mon = FindMonitor(monitorIndex);
    if (!mon) return false;
    mon->assignedSeat = seatIndex;
    return true;
}

void DeviceManager::OnDeviceChange(WPARAM wParam, LPARAM lParam) {
    if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
        LOG_INFO(L"Device change detected, re-enumerating...");
        EnumerateDevices();
        if (m_onDeviceChange) m_onDeviceChange();
    }
}

std::wstring DeviceManager::GetDevicePath(HANDLE hDevice) {
    UINT size = 0;
    GetRawInputDeviceInfoW(hDevice, RIDI_DEVICENAME, nullptr, &size);
    if (size == 0) return L"";
    std::wstring path(size, L'\0');
    GetRawInputDeviceInfoW(hDevice, RIDI_DEVICENAME, path.data(), &size);
    return path;
}

std::wstring DeviceManager::GetDeviceFriendlyName(HANDLE hDevice) {
    // Use RID_DEVICE_INFO to get device type details
    RID_DEVICE_INFO rdi = {};
    UINT size = sizeof(rdi);
    rdi.cbSize = sizeof(rdi);
    GetRawInputDeviceInfoW(hDevice, RIDI_DEVICEINFO, &rdi, &size);

    std::wstring path = GetDevicePath(hDevice);
    if (path.empty()) return L"Unknown Device";

    // Try to get friendly name via SetupDi
    // Simplified: return the device path tail
    auto pos = path.rfind(L'#');
    if (pos != std::wstring::npos)
        return path.substr(pos + 1);
    return path;
}

MonitorInfo* DeviceManager::MonitorFromPoint(POINT pt) {
    for (auto& m : m_monitors) {
        if (pt.x >= m.bounds.left && pt.x < m.bounds.right &&
            pt.y >= m.bounds.top  && pt.y < m.bounds.bottom) {
            return &m;
        }
    }
    return nullptr;
}