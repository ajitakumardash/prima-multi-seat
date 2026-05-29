// ============================================================
// Prima Multi Seat - Cursor Engine Implementation
// ============================================================

#include "../include/CursorEngine.h"

CursorEngine::CursorEngine() : m_systemCursorHidden(false) {
    m_arrowCursor = LoadCursor(nullptr, IDC_ARROW);
    for (int i = 0; i < MAX_SEATS; i++) {
        m_cursors[i].seatIndex = i;
        m_cursors[i].visible   = true;
        m_cursors[i].position  = { 0, 0 };
        m_cursors[i].clampMin  = { 0, 0 };
        m_cursors[i].clampMax  = { 1920, 1080 };
        m_cursors[i].color     = (i == 0) ? RGB(220, 50, 50) : RGB(50, 100, 220);
    }
}

CursorEngine::~CursorEngine() { Shutdown(); }

bool CursorEngine::Initialize() {
    LOG_INFO(L"CursorEngine initialized");
    return true;
}

void CursorEngine::Shutdown() {
    ShowSystemCursor();
    LOG_INFO(L"CursorEngine shut down");
}

void CursorEngine::SetSeatMonitor(int seatIndex, const RECT& bounds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return;
    m_cursors[seatIndex].clampMin = { bounds.left, bounds.top };
    m_cursors[seatIndex].clampMax = { bounds.right, bounds.bottom };
    // Start cursor in center of monitor
    m_cursors[seatIndex].position = {
        (bounds.left + bounds.right)  / 2,
        (bounds.top  + bounds.bottom) / 2
    };
}

void CursorEngine::SetCursorVisible(int seatIndex, bool visible) {
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return;
    m_cursors[seatIndex].visible = visible;
}

void CursorEngine::SetCursorColor(int seatIndex, COLORREF color) {
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return;
    m_cursors[seatIndex].color = color;
}

void CursorEngine::UpdateCursorDelta(int seatIndex, LONG dx, LONG dy) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return;
    auto& cur = m_cursors[seatIndex];
    POINT newPt = { cur.position.x + dx, cur.position.y + dy };
    cur.position = ClampToMonitor(seatIndex, newPt);
}

void CursorEngine::SetCursorPosition(int seatIndex, POINT pt) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return;
    m_cursors[seatIndex].position = ClampToMonitor(seatIndex, pt);
}

POINT CursorEngine::GetCursorPosition(int seatIndex) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return { 0, 0 };
    return m_cursors[seatIndex].position;
}

void CursorEngine::HideSystemCursor() {
    if (!m_systemCursorHidden.exchange(true)) {
        // Replace system cursor with blank cursor
        BYTE andMask[1] = { 0xFF };
        BYTE xorMask[1] = { 0x00 };
        HCURSOR hBlank  = CreateCursor(nullptr, 0, 0, 1, 1, andMask, xorMask);
        SetSystemCursor(hBlank, OCR_NORMAL);
        SetSystemCursor(hBlank, OCR_IBEAM);
        LOG_INFO(L"System cursor hidden");
    }
}

void CursorEngine::ShowSystemCursor() {
    if (m_systemCursorHidden.exchange(false)) {
        SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr, 0);
        LOG_INFO(L"System cursor restored");
    }
}

POINT CursorEngine::ClampToMonitor(int seatIndex, POINT pt) const {
    const auto& cur = m_cursors[seatIndex];
    pt.x = max(cur.clampMin.x, min(cur.clampMax.x - 1, pt.x));
    pt.y = max(cur.clampMin.y, min(cur.clampMax.y - 1, pt.y));
    return pt;
}

void CursorEngine::DrawCursor(HDC hDC, int seatIndex) {
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return;
    const auto& cur = m_cursors[seatIndex];
    if (!cur.visible) return;
    DrawArrowCursor(hDC, cur.position, cur.color);
}

void CursorEngine::DrawArrowCursor(HDC hDC, POINT pt, COLORREF color) {
    // Draw a simple colored arrow cursor via GDI
    HPEN   hPen   = CreatePen(PS_SOLID, 1, RGB(0,0,0));
    HBRUSH hBrush = CreateSolidBrush(color);
    HPEN   hOldPen   = (HPEN)SelectObject(hDC, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hDC, hBrush);

    // Arrow polygon (12 points)
    POINT arrow[] = {
        {pt.x,      pt.y     },
        {pt.x,      pt.y+16  },
        {pt.x+4,    pt.y+12  },
        {pt.x+7,    pt.y+18  },
        {pt.x+9,    pt.y+17  },
        {pt.x+6,    pt.y+11  },
        {pt.x+11,   pt.y+11  }
    };
    Polygon(hDC, arrow, 7);

    SelectObject(hDC, hOldBrush);
    SelectObject(hDC, hOldPen);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

void CursorEngine::InvalidateOverlay(int seatIndex) {
    // The overlay window redraws itself on WM_PAINT trigger
    // This is called from the main update loop
}