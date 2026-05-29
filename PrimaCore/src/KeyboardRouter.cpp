// ============================================================
// Prima Multi Seat - Keyboard Router Implementation
// ============================================================

#include "../include/KeyboardRouter.h"

KeyboardRouter::KeyboardRouter() : m_passthroughMode(false) {
    for (int i = 0; i < MAX_SEATS; i++)
        m_seatWindows[i] = nullptr;
}
KeyboardRouter::~KeyboardRouter() { Shutdown(); }

bool KeyboardRouter::Initialize() {
    LOG_INFO(L"KeyboardRouter initialized");
    return true;
}

void KeyboardRouter::Shutdown() {
    LOG_INFO(L"KeyboardRouter shut down");
}

void KeyboardRouter::MapDeviceToSeat(HANDLE hDevice, int seatIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_deviceSeatMap[hDevice] = seatIndex;
}

void KeyboardRouter::UnmapDevice(HANDLE hDevice) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_deviceSeatMap.erase(hDevice);
}

int KeyboardRouter::GetSeatForDevice(HANDLE hDevice) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_deviceSeatMap.find(hDevice);
    return (it != m_deviceSeatMap.end()) ? it->second : SEAT_NONE;
}

void KeyboardRouter::SetSeatFocusWindow(int seatIndex, HWND hWnd) {
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_seatWindows[seatIndex] = hWnd;
}

HWND KeyboardRouter::GetSeatFocusWindow(int seatIndex) const {
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return nullptr;
    return m_seatWindows[seatIndex];
}

void KeyboardRouter::SetPassthroughMode(bool enabled) {
    m_passthroughMode = enabled;
    LOG_INFO(enabled ? L"Keyboard passthrough ON" : L"Keyboard passthrough OFF");
}

void KeyboardRouter::ProcessRawInput(HRAWINPUT hRawInput) {
    UINT size = 0;
    GetRawInputData(hRawInput, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
    std::vector<BYTE> buf(size);
    if (GetRawInputData(hRawInput, RID_INPUT, buf.data(), &size,
                         sizeof(RAWINPUTHEADER)) != size) return;

    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buf.data());
    if (raw->header.dwType != RIM_TYPEKEYBOARD) return;

    HANDLE hDevice = raw->header.hDevice;
    int seat = m_passthroughMode ? SEAT_1 : GetSeatForDevice(hDevice);
    if (seat == SEAT_NONE) return;

    HWND hTarget = GetSeatFocusWindow(seat);
    if (!hTarget || !IsWindow(hTarget)) return;

    InjectKeyToWindow(hTarget, raw->data.keyboard);
}

void KeyboardRouter::InjectKeyToWindow(HWND hWnd, const RAWKEYBOARD& rk) {
    UINT vk       = rk.VKey;
    UINT scanCode = rk.MakeCode;
    bool isKeyUp  = (rk.Flags & RI_KEY_BREAK) != 0;

    UINT msg = isKeyUp ? WM_KEYUP : WM_KEYDOWN;
    LPARAM lParam = (LPARAM)(1 | (scanCode << 16));
    if (isKeyUp) lParam |= (1 << 30) | (1 << 31);

    PostMessageW(hWnd, msg, vk, lParam);

    // Also send WM_CHAR for printable characters on key down
    if (!isKeyUp) {
        BYTE keyState[256] = {};
        GetKeyboardState(keyState);
        wchar_t ch[4] = {};
        int count = ToUnicode(vk, scanCode, keyState, ch, 4, 0);
        if (count > 0 && ch[0] >= L' ') {
            PostMessageW(hWnd, WM_CHAR, (WPARAM)ch[0], lParam);
        }
    }
}