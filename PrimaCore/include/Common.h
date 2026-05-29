#pragma once
// ============================================================
// Prima Multi Seat - Common Definitions
// Shared types, constants, and utilities used across all modules
// ============================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <sstream>
#include <fstream>
#include <iostream>
#include <stdexcept>

// ─── Version ────────────────────────────────────────────────
#define PRIMA_VERSION_MAJOR 1
#define PRIMA_VERSION_MINOR 0
#define PRIMA_VERSION_PATCH 0
#define PRIMA_VERSION_STR   L"1.0.0"

// ─── Seat Definitions ────────────────────────────────────────
#define MAX_SEATS           2
#define SEAT_1              0
#define SEAT_2              1
#define SEAT_NONE          -1

// ─── IPC Named Pipe ─────────────────────────────────────────
#define IPC_PIPE_NAME       L"\\\\.\\pipe\\PrimaMultiSeat"
#define IPC_BUFFER_SIZE     4096

// ─── Hotkeys ────────────────────────────────────────────────
// Ctrl+Alt+P = Emergency failsafe restore
#define FAILSAFE_HOTKEY_ID  1001
#define FAILSAFE_MOD        (MOD_CONTROL | MOD_ALT | MOD_NOREPEAT)
#define FAILSAFE_VK         'P'

// ─── Window Messages ─────────────────────────────────────────
#define WM_PRIMA_TRAY       (WM_USER + 100)
#define WM_PRIMA_UPDATE     (WM_USER + 101)
#define WM_PRIMA_SHUTDOWN   (WM_USER + 102)

// ─── Seat State ──────────────────────────────────────────────
enum class SeatState {
    Inactive,
    Active,
    Error,
    Initializing
};

// ─── Device Types ────────────────────────────────────────────
enum class DeviceType {
    Keyboard,
    Mouse,
    Monitor,
    Audio
};

// ─── Device Info ─────────────────────────────────────────────
struct DeviceInfo {
    HANDLE      hDevice;
    DeviceType  type;
    std::wstring devicePath;
    std::wstring friendlyName;
    int          assignedSeat;  // SEAT_1, SEAT_2, or SEAT_NONE
    bool         isConnected;

    DeviceInfo()
        : hDevice(nullptr)
        , type(DeviceType::Keyboard)
        , assignedSeat(SEAT_NONE)
        , isConnected(false)
    {}
};

// ─── Monitor Info ────────────────────────────────────────────
struct MonitorInfo {
    HMONITOR    hMonitor;
    RECT        workArea;
    RECT        bounds;
    std::wstring deviceName;
    int          index;
    bool         isPrimary;
    int          assignedSeat;

    MonitorInfo()
        : hMonitor(nullptr)
        , index(0)
        , isPrimary(false)
        , assignedSeat(SEAT_NONE)
    {
        ZeroMemory(&workArea, sizeof(RECT));
        ZeroMemory(&bounds,   sizeof(RECT));
    }
};

// ─── Seat Configuration ──────────────────────────────────────
struct SeatConfig {
    int         seatIndex;
    HANDLE      keyboardDevice;
    HANDLE      mouseDevice;
    int         monitorIndex;
    std::wstring audioDeviceId;
    SeatState   state;
    POINT       cursorPos;
    COLORREF    cursorColor;

    SeatConfig()
        : seatIndex(SEAT_NONE)
        , keyboardDevice(nullptr)
        , mouseDevice(nullptr)
        , monitorIndex(-1)
        , state(SeatState::Inactive)
        , cursorColor(RGB(255, 0, 0))
    {
        cursorPos = { 0, 0 };
    }
};

// ─── IPC Message Types ───────────────────────────────────────
enum class IPCMessageType : DWORD {
    Ping            = 1,
    Pong            = 2,
    GetStatus       = 10,
    StatusResponse  = 11,
    StartSeat       = 20,
    StopSeat        = 21,
    AssignDevice    = 22,
    UpdateConfig    = 23,
    Shutdown        = 99,
    LogMessage      = 100,
};

// ─── IPC Packet ──────────────────────────────────────────────
#pragma pack(push, 1)
struct IPCPacket {
    IPCMessageType  type;
    DWORD           payloadSize;
    char            payload[IPC_BUFFER_SIZE - 8];
};
#pragma pack(pop)

// ─── Logging ─────────────────────────────────────────────────
enum class LogLevel { Debug, Info, Warning, Error };

inline void PrimaLog(LogLevel level, const std::wstring& msg) {
    const wchar_t* prefix = L"[INFO]";
    switch (level) {
        case LogLevel::Debug:   prefix = L"[DEBUG]";   break;
        case LogLevel::Warning: prefix = L"[WARN]";    break;
        case LogLevel::Error:   prefix = L"[ERROR]";   break;
        default: break;
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timeBuf[64];
    swprintf_s(timeBuf, L"%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
    OutputDebugStringW((std::wstring(timeBuf) + L" " + prefix + L" " + msg + L"\n").c_str());
}

#define LOG_INFO(msg)    PrimaLog(LogLevel::Info,    msg)
#define LOG_WARN(msg)    PrimaLog(LogLevel::Warning, msg)
#define LOG_ERROR(msg)   PrimaLog(LogLevel::Error,   msg)
#define LOG_DEBUG(msg)   PrimaLog(LogLevel::Debug,   msg)