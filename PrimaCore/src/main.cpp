// ============================================================
// Prima Multi Seat - Main Entry Point
// Win32 application with system tray icon, message loop,
// and WM_INPUT handler for Raw Input device routing.
// ============================================================

#include "../include/Common.h"
#include "../include/SeatManager.h"
#include "../include/IPCServer.h"
#include "../include/OverlayWindow.h"
#include "../include/Failsafe.h"

// ─── Forward Declarations ────────────────────────────────────
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool InitTrayIcon(HWND hWnd, HINSTANCE hInst);
void RemoveTrayIcon(HWND hWnd);
void ShowContextMenu(HWND hWnd);

// ─── Globals ─────────────────────────────────────────────────
static std::unique_ptr<SeatManager>  g_seatManager;
static std::unique_ptr<IPCServer>    g_ipcServer;
static std::unique_ptr<Failsafe>     g_failsafe;
static std::vector<std::unique_ptr<OverlayWindow>> g_overlays;
static NOTIFYICONDATA                g_nid = {};
static const wchar_t*                WINDOW_CLASS = L"PrimaMultiSeatMain";
static const wchar_t*                MUTEX_NAME   = L"PrimaMultiSeatSingleInstance";

// ─── WinMain ─────────────────────────────────────────────────
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    // ── Single instance guard ────────────────────────────────
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr,
            L"Prima Multi Seat is already running.\n"
            L"Check the system tray.",
            L"Prima Multi Seat", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    LOG_INFO(L"Prima Multi Seat v" PRIMA_VERSION_STR L" starting...");

    // ── Register window class ────────────────────────────────
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = WINDOW_CLASS;
    wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(101));
    if (!RegisterClassExW(&wc)) {
        LOG_ERROR(L"Failed to register window class");
        return 1;
    }

    // ── Create hidden message window ─────────────────────────
    HWND hWnd = CreateWindowExW(0, WINDOW_CLASS,
        L"Prima Multi Seat",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) {
        LOG_ERROR(L"Failed to create message window");
        return 1;
    }

    // ── Initialize core systems ──────────────────────────────
    g_seatManager = std::make_unique<SeatManager>();
    if (!g_seatManager->Initialize(hWnd)) {
        LOG_ERROR(L"SeatManager initialization failed");
        MessageBoxW(nullptr,
            L"Failed to initialize Prima Multi Seat.\n"
            L"Please run as Administrator.",
            L"Prima Multi Seat Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // ── Load configuration ───────────────────────────────────
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring configPath = std::wstring(exePath);
    configPath = configPath.substr(0, configPath.rfind(L'\\')) + L"\\config.json";

    if (!g_seatManager->LoadConfig(configPath)) {
        LOG_WARN(L"Config not found, applying defaults");
        g_seatManager->ApplyDefaultConfig();
        g_seatManager->SaveConfig(configPath);
    }

    // ── Create overlay windows ───────────────────────────────
    auto* dm = g_seatManager->GetDeviceManager();
    for (int i = 0; i < (int)dm->GetMonitors().size() && i < MAX_SEATS; i++) {
        auto overlay = std::make_unique<OverlayWindow>(g_seatManager->GetCursorEngine());
        if (overlay->Create(i, dm->GetMonitors()[i].bounds)) {
            g_overlays.push_back(std::move(overlay));
        }
    }

    // ── Start IPC server ─────────────────────────────────────
    g_ipcServer = std::make_unique<IPCServer>(g_seatManager.get());
    g_ipcServer->Start();

    // ── Register failsafe hotkey (Ctrl+Alt+P) ────────────────
    g_failsafe = std::make_unique<Failsafe>(g_seatManager.get());
    if (!g_failsafe->RegisterHotkey(hWnd)) {
        LOG_WARN(L"Failed to register failsafe hotkey");
    }

    // ── Tray icon ────────────────────────────────────────────
    InitTrayIcon(hWnd, hInstance);

    // ── Start seats ──────────────────────────────────────────
    g_seatManager->StartSeat(SEAT_1);
    g_seatManager->StartSeat(SEAT_2);

    LOG_INFO(L"Prima Multi Seat started successfully");

    // ── Message loop ─────────────────────────────────────────
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        g_seatManager->Update();
    }

    // ── Cleanup ──────────────────────────────────────────────
    g_failsafe->UnregisterHotkey(hWnd);
    RemoveTrayIcon(hWnd);
    g_ipcServer->Stop();
    g_seatManager->Shutdown();
    g_overlays.clear();
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);

    LOG_INFO(L"Prima Multi Seat shut down cleanly");
    return (int)msg.wParam;
}

// ─── Window Procedure ─────────────────────────────────────────
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    // ── Raw Input ────────────────────────────────────────────
    case WM_INPUT:
        if (g_seatManager) {
            g_seatManager->OnRawInput((HRAWINPUT)lParam);
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);

    // ── Failsafe Hotkey ──────────────────────────────────────
    case WM_HOTKEY:
        if (wParam == FAILSAFE_HOTKEY_ID && g_failsafe) {
            LOG_WARN(L"Failsafe hotkey triggered!");
            g_failsafe->Trigger();
        }
        return 0;

    // ── Tray Icon ────────────────────────────────────────────
    case WM_PRIMA_TRAY:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            ShowContextMenu(hWnd);
        }
        return 0;

    // ── Device Change ────────────────────────────────────────
    case WM_DEVICECHANGE:
        if (g_seatManager) {
            g_seatManager->GetDeviceManager()->OnDeviceChange(wParam, lParam);
        }
        return 0;

    // ── Display Change ───────────────────────────────────────
    case WM_DISPLAYCHANGE:
        if (g_seatManager) {
            g_seatManager->GetDeviceManager()->EnumerateMonitors();
        }
        return 0;

    case WM_PRIMA_SHUTDOWN:
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

// ─── Tray Icon Helpers ────────────────────────────────────────
bool InitTrayIcon(HWND hWnd, HINSTANCE hInst) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = hWnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_PRIMA_TRAY;
    g_nid.hIcon            = LoadIcon(hInst, MAKEINTRESOURCE(101));
    wcscpy_s(g_nid.szTip, L"Prima Multi Seat");
    return Shell_NotifyIconW(NIM_ADD, &g_nid) != FALSE;
}

void RemoveTrayIcon(HWND hWnd) {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

void ShowContextMenu(HWND hWnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 1, L"Open Dashboard");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 2, L"Start All Seats");
    AppendMenuW(hMenu, MF_STRING, 3, L"Stop All Seats");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 4, L"Failsafe Recovery");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 5, L"Exit");

    SetForegroundWindow(hWnd);
    POINT pt;
    GetCursorPos(&pt);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                              pt.x, pt.y, 0, hWnd, nullptr);
    DestroyMenu(hMenu);

    switch (cmd) {
    case 1:
        // Launch PrimaUI.exe
        ShellExecuteW(nullptr, L"open", L"PrimaUI.exe", nullptr, nullptr, SW_SHOW);
        break;
    case 2:
        if (g_seatManager) {
            g_seatManager->StartSeat(SEAT_1);
            g_seatManager->StartSeat(SEAT_2);
        }
        break;
    case 3:
        if (g_seatManager) {
            g_seatManager->StopSeat(SEAT_1);
            g_seatManager->StopSeat(SEAT_2);
        }
        break;
    case 4:
        if (g_failsafe) g_failsafe->Trigger();
        break;
    case 5:
        PostMessageW(hWnd, WM_PRIMA_SHUTDOWN, 0, 0);
        break;
    }
}