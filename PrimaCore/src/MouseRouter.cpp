// ============================================================
// Prima Multi Seat - Mouse Router Implementation
// ============================================================

#include "../include/MouseRouter.h"

MouseRouter::MouseRouter(CursorEngine* cursorEngine)
    : m_cursorEngine(cursorEngine)
    , m_passthroughMode(false)
    , m_sensitivity(1.0f)
{}
MouseRouter::~MouseRouter() { Shutdown(); }

bool MouseRouter::Initialize() {
    LOG_INFO(L"MouseRouter initialized");
    return true;
}
void MouseRouter::Shutdown() { LOG_INFO(L"MouseRouter shut down"); }

void MouseRouter::MapDeviceToSeat(HANDLE hDevice, int seatIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_deviceSeatMap[hDevice] = seatIndex;
}

void MouseRouter::UnmapDevice(HANDLE hDevice) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_deviceSeatMap.erase(hDevice);
}

int MouseRouter::GetSeatForDevice(HANDLE hDevice) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_deviceSeatMap.find(hDevice);
    return (it != m_deviceSeatMap.end()) ? it->second : SEAT_NONE;
}

void MouseRouter::SetPassthroughMode(bool enabled) {
    m_passthroughMode = enabled;
}

void MouseRouter::ProcessRawInput(HRAWINPUT hRawInput) {
    UINT size = 0;
    GetRawInputData(hRawInput, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
    std::vector<BYTE> buf(size);
    if (GetRawInputData(hRawInput, RID_INPUT, buf.data(), &size,
                         sizeof(RAWINPUTHEADER)) != size) return;

    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buf.data());
    if (raw->header.dwType != RIM_TYPEMOUSE) return;

    HANDLE hDevice = raw->header.hDevice;
    int seat = m_passthroughMode ? SEAT_1 : GetSeatForDevice(hDevice);
    if (seat == SEAT_NONE || !m_cursorEngine) return;

    const RAWMOUSE& rm = raw->data.mouse;

    // Update cursor position
    if (rm.usFlags & MOUSE_MOVE_ABSOLUTE) {
        POINT pt = { (LONG)rm.lLastX, (LONG)rm.lLastY };
        m_cursorEngine->SetCursorPosition(seat, pt);
    } else {
        LONG dx = (LONG)(rm.lLastX * m_sensitivity);
        LONG dy = (LONG)(rm.lLastY * m_sensitivity);
        m_cursorEngine->UpdateCursorDelta(seat, dx, dy);
    }

    // Inject mouse click messages
    POINT cursorPt = m_cursorEngine->GetCursorPosition(seat);
    InjectMouseMove(seat, cursorPt);

    if (rm.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
        InjectMouseClick(seat, WM_LBUTTONDOWN, MK_LBUTTON);
    if (rm.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
        InjectMouseClick(seat, WM_LBUTTONUP, 0);
    if (rm.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
        InjectMouseClick(seat, WM_RBUTTONDOWN, MK_RBUTTON);
    if (rm.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
        InjectMouseClick(seat, WM_RBUTTONUP, 0);
}

void MouseRouter::InjectMouseMove(int seatIndex, POINT screenPt) {
    HWND hWnd = WindowUnderCursor(screenPt);
    if (!hWnd || !IsWindow(hWnd)) return;
    POINT clientPt = screenPt;
    ScreenToClient(hWnd, &clientPt);
    PostMessageW(hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(clientPt.x, clientPt.y));
}

void MouseRouter::InjectMouseClick(int seatIndex, UINT message, WPARAM wParam) {
    POINT pt = m_cursorEngine->GetCursorPosition(seatIndex);
    HWND hWnd = WindowUnderCursor(pt);
    if (!hWnd || !IsWindow(hWnd)) return;
    POINT clientPt = pt;
    ScreenToClient(hWnd, &clientPt);
    PostMessageW(hWnd, message, wParam, MAKELPARAM(clientPt.x, clientPt.y));
}

HWND MouseRouter::WindowUnderCursor(POINT screenPt) {
    return WindowFromPoint(screenPt);
}