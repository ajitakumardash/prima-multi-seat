#pragma once
// ============================================================
// Prima Multi Seat - Audio Router
// Uses Windows Core Audio (WASAPI) to route audio sessions
// to per-seat audio output devices.
// ============================================================

#include "Common.h"
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>

struct AudioDevice {
    std::wstring id;
    std::wstring friendlyName;
    int          assignedSeat;
    bool         isDefault;
};

class AudioRouter {
public:
    AudioRouter();
    ~AudioRouter();

    // ─── Init/Shutdown ───────────────────────────────────────
    bool Initialize();
    void Shutdown();

    // ─── Device Enumeration ──────────────────────────────────
    void EnumerateDevices();
    const std::vector<AudioDevice>& GetDevices() const { return m_devices; }

    // ─── Routing ─────────────────────────────────────────────
    // Assign an audio output device to a seat.
    // All new audio sessions from seat processes will be
    // redirected to this device via SetDefaultAudioEndpoint.
    bool AssignDeviceToSeat(const std::wstring& deviceId, int seatIndex);

    // ─── Process Audio Routing ───────────────────────────────
    // Route audio from a specific process to seat's audio device
    bool RouteProcessAudio(DWORD processId, int seatIndex);

    // ─── Volume ──────────────────────────────────────────────
    bool SetSeatVolume(int seatIndex, float volume); // 0.0 - 1.0
    float GetSeatVolume(int seatIndex) const;

    // ─── Default Device ──────────────────────────────────────
    std::wstring GetDefaultDevice() const;

private:
    std::vector<AudioDevice>            m_devices;
    std::wstring                        m_seatDeviceIds[MAX_SEATS];
    IMMDeviceEnumerator*                m_pEnumerator;
    mutable std::mutex                  m_mutex;

    IMMDevice* GetDeviceById(const std::wstring& id);
    bool SetProcessDefaultEndpoint(DWORD pid, IMMDevice* pDevice);
};