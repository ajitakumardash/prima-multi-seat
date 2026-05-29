#pragma once
// ============================================================
// Prima Multi Seat - Overlay Window
// Per-monitor layered window (WS_EX_LAYERED | WS_EX_TRANSPARENT)
// used to render software cursors via GDI without capturing input.
// ============================================================

#include "Common.h"
#include "CursorEngine.h"

class OverlayWindow {
public:
    explicit OverlayWindow(CursorEngine* cursorEngine);
    ~OverlayWindow();

    // ─── Create one overlay per monitor ──────────────────────
    bool Create(int seatIndex, const RECT& monitorBounds);
    void Destroy();

    // ─── Rendering ───────────────────────────────────────────
    void Invalidate();
    void Update();

    HWND GetHWnd() const { return m_hWnd; }

    // ─── Message pump registration ───────────────────────────
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg,
                                     WPARAM wParam, LPARAM lParam);

private:
    HWND          m_hWnd;
    CursorEngine* m_cursorEngine;
    int           m_seatIndex;
    RECT          m_bounds;

    void Paint(HDC hDC);
    bool RegisterWindowClass();
    static const wchar_t* CLASS_NAME;
};