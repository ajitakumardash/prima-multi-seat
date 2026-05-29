# Prima Multi Seat - Architecture Deep Dive

## Overview

Prima Multi Seat uses a **layered user-mode architecture** that avoids any kernel modifications, making it stable, safe, and compatible with Windows security features including Windows Defender, Secure Boot, and Driver Signature Enforcement.

## Component Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                    USER SPACE (Ring 3)                        │
│                                                              │
│  ┌─────────────────┐         ┌──────────────────────────┐   │
│  │   PrimaUI.exe   │◄───────►│   PrimaMultiSeat.exe     │   │
│  │   (WPF/C#)      │ Named   │   (Win32 C++)             │   │
│  │   Dashboard     │  Pipe   │                           │   │
│  └─────────────────┘  IPC   │  ┌────────┐ ┌──────────┐  │   │
│                              │  │ Seat 1 │ │ Seat 2   │  │   │
│  ┌─────────────────┐         │  └────────┘ └──────────┘  │   │
│  │ PrimaService.exe│         │                           │   │
│  │ (.NET Service)  │ spawn   │  Raw Input → hDevice map  │   │
│  │ auto-restart    │────────►│  Software Cursor GDI       │   │
│  └─────────────────┘         │  Window WinEventHook       │   │
│                              │  WASAPI Audio Route        │   │
└──────────────────────────────┴──────────────────────────────┘
│
│  Windows API Layer
│  ─ RegisterRawInputDevices()   ← keyboard/mouse interception
│  ─ SetWinEventHook()           ← window move tracking
│  ─ IMMDeviceEnumerator         ← audio device enumeration
│  ─ EnumDisplayMonitors()       ← monitor layout
│  ─ CreateNamedPipe()           ← IPC channel
└──────────────────────────────────────────────────────────────
```

## Raw Input Pipeline

```
Physical Device (USB HID)
        │
        ▼
Windows HID Driver (kernel) — SAFE, standard driver
        │  
        ▼
WM_INPUT message → PrimaMultiSeat.exe message window
        │
        ▼
GetRawInputData(RID_INPUT)
        │
   ┌────┴────┐
   │         │
Keyboard    Mouse
   │         │
   ▼         ▼
hDevice   hDevice
lookup     lookup
   │         │
   ▼         ▼
Seat 1   Seat 1
or       or
Seat 2   Seat 2
   │         │
   ▼         ▼
PostMessage  UpdateCursorDelta()
(WM_KEY*)    ClampToMonitor()
to focus     → GDI Overlay Paint
window
```

## Cursor Engine

The cursor system uses **layered click-through windows**:

1. Two `HWND` windows created with `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST`
2. These windows sit on top of everything but pass through all mouse clicks
3. `SetLayeredWindowAttributes()` uses color-key transparency (magenta = transparent)
4. GDI arrow polygons are drawn at the cursor position on each `WM_PAINT`
5. The system cursor is replaced with a 1×1 invisible cursor via `SetSystemCursor()`

## Window Isolation

Uses `SetWinEventHook` with `EVENT_SYSTEM_MOVESIZEEND`:
- When a window finishes moving, the isolator checks if the window center is within the seat's monitor bounds
- If not, the window is moved back to its last valid position via `MoveWindow()`
- This is a **cooperative** approach — determined users can still move windows

## Audio Routing

WASAPI is used for audio device enumeration and volume control:
- `IMMDeviceEnumerator::EnumAudioEndpoints()` finds all render devices
- Each seat is assigned a specific `IMMDevice` endpoint
- Per-application audio routing uses the Windows Audio Session API (WASAPI session management)

## IPC Protocol

Named pipe (`\\.\pipe\PrimaMultiSeat`) with binary packet format:
```
[4 bytes: MessageType] [4 bytes: PayloadSize] [4088 bytes: Payload]
```

Message types: Ping, GetStatus, StartSeat, StopSeat, AssignDevice, Shutdown