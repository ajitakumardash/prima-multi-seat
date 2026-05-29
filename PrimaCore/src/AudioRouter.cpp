// ============================================================
// Prima Multi Seat - Audio Router Implementation
// WASAPI-based audio device routing per seat
// ============================================================

#include "../include/AudioRouter.h"
#include <Functiondiscoverykeys_devpkey.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

AudioRouter::AudioRouter() : m_pEnumerator(nullptr) {}
AudioRouter::~AudioRouter() { Shutdown(); }

bool AudioRouter::Initialize() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        LOG_ERROR(L"COM initialization failed");
        return false;
    }

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                           CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                           (void**)&m_pEnumerator);
    if (FAILED(hr)) {
        LOG_ERROR(L"Failed to create IMMDeviceEnumerator");
        return false;
    }

    EnumerateDevices();
    LOG_INFO(L"AudioRouter initialized");
    return true;
}

void AudioRouter::Shutdown() {
    if (m_pEnumerator) {
        m_pEnumerator->Release();
        m_pEnumerator = nullptr;
    }
    CoUninitialize();
    LOG_INFO(L"AudioRouter shut down");
}

void AudioRouter::EnumerateDevices() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_devices.clear();
    if (!m_pEnumerator) return;

    IMMDeviceCollection* pCollection = nullptr;
    HRESULT hr = m_pEnumerator->EnumAudioEndpoints(
        eRender, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) return;

    UINT count = 0;
    pCollection->GetCount(&count);
    for (UINT i = 0; i < count; i++) {
        IMMDevice* pDevice = nullptr;
        pCollection->Item(i, &pDevice);
        if (!pDevice) continue;

        LPWSTR pwszId = nullptr;
        pDevice->GetId(&pwszId);

        AudioDevice dev;
        if (pwszId) {
            dev.id = pwszId;
            CoTaskMemFree(pwszId);
        }
        dev.assignedSeat = SEAT_NONE;

        // Get friendly name
        IPropertyStore* pStore = nullptr;
        if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pStore))) {
            PROPVARIANT varName;
            PropVariantInit(&varName);
            if (SUCCEEDED(pStore->GetValue(PKEY_Device_FriendlyName, &varName))) {
                if (varName.vt == VT_LPWSTR)
                    dev.friendlyName = varName.pwszVal;
            }
            PropVariantClear(&varName);
            pStore->Release();
        }

        m_devices.push_back(dev);
        LOG_INFO(L"Audio device: " + dev.friendlyName);
        pDevice->Release();
    }
    pCollection->Release();
}

bool AudioRouter::AssignDeviceToSeat(const std::wstring& deviceId, int seatIndex) {
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_seatDeviceIds[seatIndex] = deviceId;
    for (auto& dev : m_devices) {
        if (dev.id == deviceId) dev.assignedSeat = seatIndex;
    }
    LOG_INFO(L"Audio device assigned to seat " + std::to_wstring(seatIndex + 1));
    return true;
}

bool AudioRouter::SetSeatVolume(int seatIndex, float volume) {
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return false;
    if (!m_pEnumerator) return false;
    IMMDevice* pDev = GetDeviceById(m_seatDeviceIds[seatIndex]);
    if (!pDev) return false;
    IAudioEndpointVolume* pVol = nullptr;
    HRESULT hr = pDev->Activate(__uuidof(IAudioEndpointVolume),
                                  CLSCTX_ALL, nullptr, (void**)&pVol);
    if (SUCCEEDED(hr) && pVol) {
        pVol->SetMasterVolumeLevelScalar(volume, nullptr);
        pVol->Release();
    }
    pDev->Release();
    return SUCCEEDED(hr);
}

float AudioRouter::GetSeatVolume(int seatIndex) const {
    if (seatIndex < 0 || seatIndex >= MAX_SEATS) return 1.0f;
    return 1.0f; // simplified
}

IMMDevice* AudioRouter::GetDeviceById(const std::wstring& id) {
    if (id.empty() || !m_pEnumerator) return nullptr;
    IMMDevice* pDevice = nullptr;
    m_pEnumerator->GetDevice(id.c_str(), &pDevice);
    return pDevice;
}

bool AudioRouter::RouteProcessAudio(DWORD processId, int seatIndex) {
    // Per-process audio routing would require Windows 10 AudioGraph API
    // or undocumented SetDefaultAudioEndpoint per-process
    // This is a best-effort placeholder using session management
    LOG_INFO(L"Audio route request for PID " + std::to_wstring(processId));
    return true;
}

std::wstring AudioRouter::GetDefaultDevice() const {
    if (!m_pEnumerator) return L"";
    IMMDevice* pDevice = nullptr;
    m_pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    if (!pDevice) return L"";
    LPWSTR pwszId = nullptr;
    pDevice->GetId(&pwszId);
    std::wstring id = pwszId ? pwszId : L"";
    CoTaskMemFree(pwszId);
    pDevice->Release();
    return id;
}