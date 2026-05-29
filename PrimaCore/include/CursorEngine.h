#pragma once
// ============================================================
// Prima Multi Seat - Cursor Engine
// Manages dual software-rendered cursors, one per seat/monitor.
// Hides the system cursor and draws custom overlays via GDI.
// ============================================================

#include "Common.h"

struct CursorState {
    int     seatIndex;
    POINT   position;
    POINT   clampMin;   // Monitor top-left
    POINT   clampMax;   // Monitor bottom-right
    HCURSOR hCursor;
    bool    visible;
    COLORREF color;
};

class CursorEngine {
public:
    CursorEngine();
    ~CursorEngine();

    // ─── Init/Shutdown ───────────────────────────────────────
    bool Initialize();
    void Shutdown();

    // ─── Cursor Setup ────────────────────────────────────────
    void SetSeatMonitor(int seatIndex, const RECT& monitorBounds);
    void SetCursorVisible(int seatIndex, bool visible);
    void SetCursorColor(int seatIndex, COLORREF color);

    // ─── Position Updates ────────────────────────────────────
    // Called with raw mouse delta from RawInput
    void UpdateCursorDelta(int seatIndex, LONG dx, LONG dy);
    void SetCursorPosition(int seatIndex, POINT pt);
    POINT GetCursorPosition(int seatIndex) const;

    // ─── System Cursor ───────────────────────────────────────
    void HideSystemCursor();
    void ShowSystemCursor();

    // ─── Rendering ───────────────────────────────────────────
    // Called from overlay window paint cycle
    void DrawCursor(HDC hDC, int seatIndex);
    void InvalidateOverlay(int seatIndex);

private:
    CursorState         m_cursors[MAX_SEATS];
    HCURSOR             m_arrowCursor;
    std::atomic<bool>   m_systemCursorHidden;
    mutable std::mutex  m_mutex;

    // ─── Helpers ─────────────────────────────────────────────
    POINT ClampToMonitor(int seatIndex, POINT pt) const;
    void  DrawArrowCursor(HDC hDC, POINT pt, COLORREF color);
    HBITMAP CreateCursorBitmap(COLORREF color);
};