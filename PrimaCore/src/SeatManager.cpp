// ============================================================
// Prima Multi Seat - Seat Manager Implementation
// ============================================================

#include "../include/SeatManager.h"

SeatManager::SeatManager() : m_messageWindow(nullptr), m_initialized(false) {
    for (int i = 0; i < MAX_SEATS; i++) {
        m_seats[i].seatIndex = i;
        m_seats[i].state     = SeatState::Inactive;
    }
}

SeatManager::~SeatManager() { Shutdown(); }

bool SeatManager::Initialize(HWND messageWindow) {
    m_messageWindow = messageWindow;

    m_configManager  = std::make_unique<ConfigManager>();
    m_deviceManager  = std::make_unique<DeviceManager>();
    m_cursorEngine   = std::make_unique<CursorEngine>();
    m_keyboardRouter = std::make_unique<KeyboardRouter>();
    m_mouseRouter    = std::make_unique<MouseRouter>(m_cursorEngine.get());
    m_windowIsolator = std::make_unique<WindowIsolator>();
    m_audioRouter    = std::make_unique<AudioRouter>();

    if (!m_deviceManager->Initialize(messageWindow)) return false;

    m_deviceManager->SetDeviceChangeCallback([this]() { OnDeviceChange(); });

    if (!m_cursorEngine->Initialize())   return false;
    if (!m_keyboardRouter->Initialize()) return false;
    if (!m_mouseRouter->Initialize())    return false;
    if (!m_windowIsolator->Initialize()) return false;
    if (!m_audioRouter->Initialize())    return false;

    m_initialized = true;
    LOG_INFO(L"SeatManager initialized");
    return true;
}

void SeatManager::Shutdown() {
    if (!m_initialized) return;
    for (int i = 0; i < MAX_SEATS; i++) StopSeat(i);
    if (m_audioRouter)    m_audioRouter->Shutdown();
    if (m_windowIsolator) m_windowIsolator->Shutdown();
    if (m_mouseRouter)    m_mouseRouter->Shutdown();
    if (m_keyboardRouter) m_keyboardRouter->Shutdown();
    if (m_cursorEngine)   m_cursorEngine->Shutdown();
    if (m_deviceManager)  m_deviceManager->Shutdown();
    m_initialized = false;
    LOG_INFO(L"SeatManager shut down");
}

bool SeatManager::StartSeat(int seatIndex) {
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_seats[seatIndex].state = SeatState::Active;
    LOG_INFO(L"Seat " + std::to_wstring(seatIndex + 1) + L" started");
    return true;
}

bool SeatManager::StopSeat(int seatIndex) {
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_seats[seatIndex].state = SeatState::Inactive;
    LOG_INFO(L"Seat " + std::to_wstring(seatIndex + 1) + L" stopped");
    return true;
}

bool SeatManager::IsSeatActive(int seatIndex) const {
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return false;
    return m_seats[seatIndex].state == SeatState::Active;
}

SeatState SeatManager::GetSeatState(int seatIndex) const {
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return SeatState::Error;
    return m_seats[seatIndex].state;
}

void SeatManager::OnRawInput(HRAWINPUT hRawInput) {
    UINT size = 0;
    GetRawInputData(hRawInput, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
    std::vector<BYTE> buf(size);
    if (GetRawInputData(hRawInput, RID_INPUT, buf.data(), &size,
                         sizeof(RAWINPUTHEADER)) != size) return;

    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buf.data());

    if (raw->header.dwType == RIM_TYPEKEYBOARD) {
        m_keyboardRouter->ProcessRawInput(hRawInput);
    }
    else if (raw->header.dwType == RIM_TYPEMOUSE) {
        m_mouseRouter->ProcessRawInput(hRawInput);
    }
}

void SeatManager::Update() {
    UpdateSeatCursors();
}

void SeatManager::UpdateSeatCursors() {
    for (int i = 0; i < MAX_SEATS; i++) {
        if (m_seats[i].state == SeatState::Active) {
            m_cursorEngine->InvalidateOverlay(i);
        }
    }
}

bool SeatManager::LoadConfig(const std::wstring& configPath) {
    if (!m_configManager->Load(configPath)) return false;
    const auto& cfg = m_configManager->GetConfig();

    for (int i = 0; i < MAX_SEATS; i++) {
        m_seats[i].monitorIndex = cfg.seats[i].monitorIndex;
        m_seats[i].cursorColor  = cfg.seats[i].cursorColor;

        // Apply monitor assignment
        auto* mon = m_deviceManager->FindMonitor(cfg.seats[i].monitorIndex);
        if (mon) {
            m_cursorEngine->SetSeatMonitor(i, mon->bounds);
            m_windowIsolator->SetSeatBounds(i, mon->bounds);
        }

        // Apply audio assignment
        if (!cfg.seats[i].audioDeviceId.empty()) {
            m_audioRouter->AssignDeviceToSeat(cfg.seats[i].audioDeviceId, i);
        }
    }

    return true;
}

bool SeatManager::SaveConfig(const std::wstring& configPath) {
    return m_configManager->Save(configPath);
}

void SeatManager::ApplyDefaultConfig() {
    m_configManager->SetDefaults();
    const auto& cfg = m_configManager->GetConfig();
    for (int i = 0; i < MAX_SEATS; i++) {
        m_seats[i].monitorIndex = i;
        auto* mon = m_deviceManager->FindMonitor(i);
        if (mon) {
            m_cursorEngine->SetSeatMonitor(i, mon->bounds);
            m_windowIsolator->SetSeatBounds(i, mon->bounds);
        }
    }

    // Auto-assign first keyboard/mouse to seat 1, second to seat 2
    const auto& kbs = m_deviceManager->GetKeyboards();
    const auto& mice = m_deviceManager->GetMice();
    for (int i = 0; i < MAX_SEATS && i < (int)kbs.size(); i++) {
        AssignKeyboardToSeat(kbs[i].hDevice, i);
    }
    for (int i = 0; i < MAX_SEATS && i < (int)mice.size(); i++) {
        AssignMouseToSeat(mice[i].hDevice, i);
    }
}

bool SeatManager::AssignKeyboardToSeat(HANDLE hDevice, int seatIndex) {
    m_deviceManager->AssignDeviceToSeat(hDevice, seatIndex);
    m_keyboardRouter->MapDeviceToSeat(hDevice, seatIndex);
    m_seats[seatIndex].keyboardDevice = hDevice;
    return true;
}

bool SeatManager::AssignMouseToSeat(HANDLE hDevice, int seatIndex) {
    m_deviceManager->AssignDeviceToSeat(hDevice, seatIndex);
    m_mouseRouter->MapDeviceToSeat(hDevice, seatIndex);
    m_seats[seatIndex].mouseDevice = hDevice;
    return true;
}

bool SeatManager::AssignMonitorToSeat(int monitorIndex, int seatIndex) {
    m_deviceManager->AssignMonitorToSeat(monitorIndex, seatIndex);
    auto* mon = m_deviceManager->FindMonitor(monitorIndex);
    if (mon) {
        m_cursorEngine->SetSeatMonitor(seatIndex, mon->bounds);
        m_windowIsolator->SetSeatBounds(seatIndex, mon->bounds);
        m_seats[seatIndex].monitorIndex = monitorIndex;
    }
    return true;
}

bool SeatManager::AssignAudioToSeat(const std::wstring& deviceId, int seatIndex) {
    return m_audioRouter->AssignDeviceToSeat(deviceId, seatIndex);
}

void SeatManager::OnDeviceChange() {
    LOG_INFO(L"Device change: re-applying assignments");
    ApplyDefaultConfig();
}

std::wstring SeatManager::GetStatusJSON() const {
    std::wostringstream o;
    o << L"{\n";
    o << L"  \"seats\": [\n";
    for (int i = 0; i < MAX_SEATS; i++) {
        const auto& s = m_seats[i];
        o << L"    {";
        o << L"\"index\":" << i << L",";
        o << L"\"active\":" << (s.state == SeatState::Active ? L"true" : L"false") << L",";
        o << L"\"monitor\":" << s.monitorIndex;
        o << L"}";
        if (i < MAX_SEATS - 1) o << L",";
        o << L"\n";
    }
    o << L"  ]\n}\n";
    return o.str();
}