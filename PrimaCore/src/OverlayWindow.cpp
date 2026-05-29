// ============================================================
// Prima Multi Seat - Overlay Window Implementation
// Layered, click-through window for cursor rendering
// ============================================================

#include "../include/OverlayWindow.h"

const wchar_t* OverlayWindow::CLASS_NAME = L"PrimaOverlay";

OverlayWindow::OverlayWindow(CursorEngine* cursorEngine)
    : m_hWnd(nullptr)
    , m_cursorEngine(cursorEngine)
    , m_seatIndex(-1)
{
    ZeroMemory(&m_bounds, sizeof(RECT));
}

OverlayWindow::~OverlayWindow() { Destroy(); }

bool OverlayWindow::RegisterWindowClass() {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.hCursor       = nullptr;
    RegisterClassExW(&wc); // ignore if already registered
    return true;
}

bool OverlayWindow::Create(int seatIndex, const RECT& monitorBounds) {
    m_seatIndex = seatIndex;
    m_bounds    = monitorBounds;

    RegisterWindowClass();

    int w = monitorBounds.right  - monitorBounds.left;
    int h = monitorBounds.bottom - monitorBounds.top;

    // WS_EX_LAYERED | WS_EX_TRANSPARENT = click-through layered window
    m_hWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        CLASS_NAME,
        L"PrimaOverlay",
        WS_POPUP,
        monitorBounds.left, monitorBounds.top, w, h,
        nullptr, nullptr, GetModuleHandleW(nullptr), this
    );

    if (!m_hWnd) {
        LOG_ERROR(L"Failed to create overlay window for seat " + std::to_wstring(seatIndex));
        return false;
    }

    // Set alpha = 0 background, key = magenta for transparency
    SetLayeredWindowAttributes(m_hWnd, RGB(255,0,255), 0, LWA_COLORKEY);
    ShowWindow(m_hWnd, SW_SHOWNOACTIVATE);
    UpdateWindow(m_hWnd);

    LOG_INFO(L"Overlay window created for seat " + std::to_wstring(seatIndex));
    return true;
}

void OverlayWindow::Destroy() {
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

void OverlayWindow::Invalidate() {
    if (m_hWnd) InvalidateRect(m_hWnd, nullptr, TRUE);
}

void OverlayWindow::Update() {
    if (m_hWnd) {
        InvalidateRect(m_hWnd, nullptr, TRUE);
        UpdateWindow(m_hWnd);
    }
}

void OverlayWindow::Paint(HDC hDC) {
    // Fill with transparent color key (magenta)
    RECT rc;
    GetClientRect(m_hWnd, &rc);
    HBRUSH hBrush = CreateSolidBrush(RGB(255, 0, 255));
    FillRect(hDC, &rc, hBrush);
    DeleteObject(hBrush);

    // Draw cursor for this seat
    if (m_cursorEngine) {
        POINT cursorPt = m_cursorEngine->GetCursorPosition(m_seatIndex);
        // Convert screen coordinates to client coordinates
        cursorPt.x -= m_bounds.left;
        cursorPt.y -= m_bounds.top;
        m_cursorEngine->DrawCursor(hDC, m_seatIndex);
    }
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hWnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam)
{
    OverlayWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<OverlayWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<OverlayWindow*>(
            GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hWnd, msg, wParam, lParam);

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(hWnd, &ps);
        self->Paint(hDC);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1; // We handle background in WM_PAINT
    case WM_DESTROY:
        self->m_hWnd = nullptr;
        return 0;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}