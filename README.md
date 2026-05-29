# Prima Multi Seat

> **Professional Dual-Seat Software for Windows 10/11 School Labs**
> One PC. Two monitors. Two keyboards. Two mice. Two users — simultaneously.

[![Build](https://github.com/yourusername/prima-multi-seat/actions/workflows/build.yml/badge.svg)](https://github.com/yourusername/prima-multi-seat/actions/workflows/build.yml)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11%20x64-blue)](https://github.com/yourusername/prima-multi-seat)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)
[![Release](https://img.shields.io/github/v/release/yourusername/prima-multi-seat)](https://github.com/yourusername/prima-multi-seat/releases)

---

## 🖥️ What is Prima Multi Seat?

Prima Multi Seat is a **user-mode dual-seat software** solution that enables a single Windows 10/11 PC to support **two independent workstations** simultaneously:

| Feature | Details |
|---------|---------|
| 🖥 Dual Monitors | Each seat gets its own display |
| ⌨ Dual Keyboards | Independently routed via Raw Input API |
| 🖱 Dual Mice | Independent software cursors per monitor |
| 🔊 Dual Audio | Per-seat audio device routing via WASAPI |
| 🔒 Window Isolation | Apps locked to assigned monitor |
| ⚡ Failsafe | Ctrl+Alt+P emergency recovery |

> **Designed for:** School computer labs, training centers, public kiosks, dual-user workstations

---

## 📦 Downloads

### Option A: GitHub Actions (Recommended)
1. Fork this repository
2. Go to **Actions → Build Prima Multi Seat → Run workflow**
3. Download artifacts from the completed run

### Option B: GitHub Releases
Download pre-built binaries from the [Releases page](https://github.com/yourusername/prima-multi-seat/releases).

| File | Description |
|------|-------------|
| `PrimaMultiSeatSetup.exe` | Full Windows Installer |
| `PrimaMultiSeat_Portable.zip` | Portable version (no install) |

---

## 🚀 Quick Start

### Requirements
- Windows 10 (version 1809+) or Windows 11 — x64 only
- [.NET 8.0 Runtime](https://dotnet.microsoft.com/download/dotnet/8.0) (x64)
- Administrator account
- 2 physical monitors connected
- 2 USB keyboards
- 2 USB mice
- (Optional) 2 separate audio output devices

### Installation (Installer)
```
1. Download PrimaMultiSeatSetup.exe
2. Right-click → Run as Administrator
3. Follow the installer wizard
4. Reboot if prompted
5. Launch "Prima Multi Seat" from Start Menu
```

### Portable Version
```
1. Download PrimaMultiSeat_Portable.zip
2. Extract to any folder (e.g., C:\PrimaMultiSeat)
3. Right-click PrimaMultiSeat.exe → Run as Administrator
4. Open PrimaUI.exe for the dashboard
```

---

## ⚙️ Configuration

Edit `config.json` in the installation directory:

```json
{
  "version": "1.0.0",
  "general": {
    "cursorSensitivity": 1.0,
    "enableAudioRouting": true,
    "enableWindowIsolation": true,
    "enableOverlayCursors": true
  },
  "seats": [
    {
      "seatIndex": 0,
      "monitorIndex": 0,
      "keyboardPath": "auto",
      "mousePath": "auto",
      "audioDeviceId": "auto"
    },
    {
      "seatIndex": 1,
      "monitorIndex": 1,
      "keyboardPath": "auto",
      "mousePath": "auto",
      "audioDeviceId": "auto"
    }
  ]
}
```

### Device Assignment
Open **PrimaUI.exe → Devices** to assign specific keyboards, mice, and audio devices to each seat using the GUI.

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────┐
│                  PrimaUI.exe (WPF)                   │
│  Dashboard · Device Assignment · Monitor Preview     │
└─────────────────┬───────────────────────────────────┘
                  │ Named Pipe IPC
┌─────────────────▼───────────────────────────────────┐
│            PrimaMultiSeat.exe (C++ Win32)            │
│                                                      │
│  ┌───────────────┐  ┌──────────────┐                 │
│  │ DeviceManager │  │ SeatManager  │                 │
│  │ (Raw Input)   │  │ (Coordinator)│                 │
│  └───────────────┘  └──────────────┘                 │
│                                                      │
│  ┌──────────────┐   ┌──────────────┐                 │
│  │KeyboardRouter│   │ MouseRouter  │                 │
│  │(hDevice map) │   │ (delta → pos)│                 │
│  └──────────────┘   └──────────────┘                 │
│                                                      │
│  ┌──────────────┐   ┌──────────────┐                 │
│  │CursorEngine  │   │WindowIsolator│                 │
│  │(GDI overlay) │   │(WinEventHook)│                 │
│  └──────────────┘   └──────────────┘                 │
│                                                      │
│  ┌──────────────┐   ┌──────────────┐                 │
│  │AudioRouter   │   │  Failsafe    │                 │
│  │(WASAPI)      │   │(Ctrl+Alt+P)  │                 │
│  └──────────────┘   └──────────────┘                 │
└─────────────────────────────────────────────────────┘
                  │ Service Control
┌─────────────────▼───────────────────────────────────┐
│          PrimaService.exe (.NET 8 Windows Service)   │
│      Auto-start · Auto-restart · Recovery            │
└─────────────────────────────────────────────────────┘
```

---

## 🔨 Building from Source

### Prerequisites
- Visual Studio 2022 (with C++ Desktop workload + Windows SDK 10.0)
- .NET 8.0 SDK
- Inno Setup 6 (for installer)
- Git

### Build Steps
```powershell
# 1. Clone
git clone https://github.com/yourusername/prima-multi-seat.git
cd prima-multi-seat

# 2. Build C++ Core
cd PrimaCore
msbuild PrimaCore.vcxproj /p:Configuration=Release /p:Platform=x64

# 3. Build .NET Service
cd ..\PrimaService
dotnet publish -c Release -r win-x64 --self-contained false -o ..\Build\Release

# 4. Build WPF UI
cd ..\PrimaUI
dotnet publish -c Release -r win-x64 --self-contained false -o ..\Build\Release

# 5. Build Installer (optional)
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" Installer\PrimaMultiSeatSetup.iss
```

### Using GitHub Actions
1. Push to `main` branch
2. Actions automatically build all projects
3. Download artifacts from Actions tab

---

## 📋 How Device Routing Works

### Keyboard Routing
1. All keyboards are registered with `RegisterRawInputDevices()` using `RIDEV_INPUTSINK | RIDEV_NOLEGACY`
2. `WM_INPUT` messages arrive at the hidden message window
3. Each keyboard is identified by its `HANDLE hDevice`
4. Input is injected via `PostMessage(WM_KEYDOWN/WM_KEYUP)` to the focused window of the assigned seat

### Mouse Routing  
1. Each mouse is tracked by `hDevice` handle
2. Raw delta (`lLastX`, `lLastY`) moves the software cursor
3. Software cursor is clamped to the assigned monitor bounds
4. Click events injected via `PostMessage` to the window under the software cursor

### Cursor System
1. System cursor is replaced with a 1×1 invisible cursor
2. Two layered windows (`WS_EX_LAYERED | WS_EX_TRANSPARENT`) sit on each monitor
3. GDI-drawn colored arrows render the per-seat cursors
4. Red cursor = Seat 1, Blue cursor = Seat 2

---

## 🆘 Failsafe Recovery

If anything goes wrong, press **Ctrl+Alt+P** to:
- Disable all input routing
- Restore the system cursor
- Show the recovery dialog
- Option to restart or exit

You can also run the Stop script:
```
stop-prima.bat  (Run as Administrator)
```

---

## 🔧 Troubleshooting

| Problem | Solution |
|---------|----------|
| "Access Denied" on start | Run as Administrator |
| Cursor not showing | Check overlay windows in Task Manager |
| Keyboard not routed | Open PrimaUI → Devices, reassign keyboard |
| Audio not separated | Verify two audio devices in Device Manager |
| Windows moving to wrong monitor | Enable Window Isolation in Settings |
| Service not starting | Run `install-service.bat` as Administrator |
| Black overlay windows | Update display drivers |

### Logs
Check logs at: `%InstallDir%\logs\prima.log`

### Event Log
```
Event Viewer → Windows Logs → Application → Source: PrimaMultiSeat
```

---

## ⚠️ Limitations

- **Shared Windows session** — not true multi-user (no separate user accounts per seat)
- **App isolation is per-monitor** — some apps may ignore window position enforcement
- **No GPU isolation** — both seats share the same GPU
- **Audio routing** may not work for all apps (depends on audio API used by app)
- **Tested on** Windows 10 21H2, Windows 11 22H2, Windows 11 23H2

---

## 🗂️ Repository Structure

```
prima-multi-seat/
├── PrimaCore/              # C++ Win32 Core Engine
│   ├── include/            # Header files
│   ├── src/                # Source files
│   ├── PrimaCore.vcxproj   # VS project
│   └── app.manifest        # UAC + DPI manifest
├── PrimaService/           # C# .NET 8 Windows Service
│   ├── PrimaWorker.cs      # Background worker
│   ├── ServiceInstaller.cs # Install/uninstall helpers
│   └── PrimaService.csproj
├── PrimaUI/                # C# WPF Dashboard
│   ├── Views/              # XAML pages
│   ├── ViewModels/         # MVVM view models
│   ├── Services/           # IPC client
│   ├── Themes/             # Dark theme XAML
│   └── PrimaUI.csproj
├── Installer/              # Inno Setup installer
│   └── PrimaMultiSeatSetup.iss
├── Scripts/                # Batch scripts
│   ├── start-prima.bat
│   ├── stop-prima.bat
│   ├── install-service.bat
│   └── uninstall-service.bat
├── Docs/                   # Documentation
├── .github/workflows/      # GitHub Actions
│   └── build.yml
├── config.json             # Default configuration
├── PrimaMultiSeat.sln      # Visual Studio Solution
└── README.md
```

---

## 📄 License

MIT License — see [LICENSE](Installer/LICENSE.txt) for details.

---

## 🙏 Acknowledgments

- Microsoft Raw Input API documentation
- WASAPI Core Audio documentation  
- Inno Setup by Jordan Russell
- WPF Dark theme inspiration from VS Code