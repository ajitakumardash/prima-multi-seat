#pragma once
// ============================================================
// Prima Multi Seat - Config Manager
// JSON configuration file read/write using a minimal
// hand-written JSON parser (no external dependencies).
// ============================================================

#include "Common.h"

struct PrimaConfig {
    // ─── Seat 1 ──────────────────────────────────────────────
    struct SeatCfg {
        std::wstring keyboardPath;
        std::wstring mousePath;
        int          monitorIndex;
        std::wstring audioDeviceId;
        COLORREF     cursorColor;
    } seats[MAX_SEATS];

    // ─── General ─────────────────────────────────────────────
    float   cursorSensitivity;
    bool    startMinimized;
    bool    startWithWindows;
    bool    enableAudioRouting;
    bool    enableWindowIsolation;
    bool    enableOverlayCursors;
    std::wstring logPath;

    PrimaConfig() {
        seats[0].monitorIndex   = 0;
        seats[0].cursorColor    = RGB(220, 50,  50);   // Red
        seats[1].monitorIndex   = 1;
        seats[1].cursorColor    = RGB(50,  100, 220);  // Blue
        cursorSensitivity       = 1.0f;
        startMinimized          = false;
        startWithWindows        = false;
        enableAudioRouting      = true;
        enableWindowIsolation   = true;
        enableOverlayCursors    = true;
        logPath                 = L"logs\\prima.log";
    }
};

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    // ─── Load / Save ─────────────────────────────────────────
    bool Load(const std::wstring& path);
    bool Save(const std::wstring& path) const;
    void SetDefaults();

    // ─── Access ──────────────────────────────────────────────
    const PrimaConfig& GetConfig() const { return m_config; }
    PrimaConfig&       GetConfig()       { return m_config; }

private:
    PrimaConfig  m_config;
    std::wstring m_path;

    // ─── Minimal JSON helpers ────────────────────────────────
    std::string  WStringToUTF8(const std::wstring& ws) const;
    std::wstring UTF8ToWString(const std::string& s)   const;
    std::string  BuildJSON() const;
    bool         ParseJSON(const std::string& json);
    std::string  Escape(const std::string& s) const;
};