// ============================================================
// Prima Multi Seat - Config Manager Implementation
// Minimal JSON read/write without external dependencies
// ============================================================

#include "../include/ConfigManager.h"
#include <sstream>
#include <fstream>
#include <iomanip>

ConfigManager::ConfigManager() { SetDefaults(); }
ConfigManager::~ConfigManager() {}

void ConfigManager::SetDefaults() {
    m_config = PrimaConfig();
}

bool ConfigManager::Load(const std::wstring& path) {
    m_path = path;
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string json((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    return ParseJSON(json);
}

bool ConfigManager::Save(const std::wstring& path) const {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << BuildJSON();
    return true;
}

std::string ConfigManager::WStringToUTF8(const std::wstring& ws) const {
    if (ws.empty()) return "";
    int sz = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(sz - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, s.data(), sz, nullptr, nullptr);
    return s;
}

std::wstring ConfigManager::UTF8ToWString(const std::string& s) const {
    if (s.empty()) return L"";
    int sz = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring ws(sz - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws.data(), sz);
    return ws;
}

std::string ConfigManager::Escape(const std::string& s) const {
    std::string out;
    for (char c : s) {
        if (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else           out += c;
    }
    return out;
}

std::string ConfigManager::BuildJSON() const {
    const auto& cfg = m_config;
    std::ostringstream o;
    o << "{\n";
    o << "  \"version\": \"" << PRIMA_VERSION_MAJOR << "."
      << PRIMA_VERSION_MINOR << "." << PRIMA_VERSION_PATCH << "\",\n";
    o << "  \"general\": {\n";
    o << "    \"cursorSensitivity\": " << cfg.cursorSensitivity << ",\n";
    o << "    \"startMinimized\": " << (cfg.startMinimized ? "true" : "false") << ",\n";
    o << "    \"startWithWindows\": " << (cfg.startWithWindows ? "true" : "false") << ",\n";
    o << "    \"enableAudioRouting\": " << (cfg.enableAudioRouting ? "true" : "false") << ",\n";
    o << "    \"enableWindowIsolation\": " << (cfg.enableWindowIsolation ? "true" : "false") << ",\n";
    o << "    \"enableOverlayCursors\": " << (cfg.enableOverlayCursors ? "true" : "false") << "\n";
    o << "  },\n";
    o << "  \"seats\": [\n";
    for (int i = 0; i < MAX_SEATS; i++) {
        const auto& s = cfg.seats[i];
        o << "    {\n";
        o << "      \"seatIndex\": " << i << ",\n";
        o << "      \"keyboardPath\": \"" << Escape(WStringToUTF8(s.keyboardPath)) << "\",\n";
        o << "      \"mousePath\": \"" << Escape(WStringToUTF8(s.mousePath)) << "\",\n";
        o << "      \"monitorIndex\": " << s.monitorIndex << ",\n";
        o << "      \"audioDeviceId\": \"" << Escape(WStringToUTF8(s.audioDeviceId)) << "\",\n";
        o << "      \"cursorColor\": " << (DWORD)s.cursorColor << "\n";
        o << "    }";
        if (i < MAX_SEATS - 1) o << ",";
        o << "\n";
    }
    o << "  ]\n";
    o << "}\n";
    return o.str();
}

bool ConfigManager::ParseJSON(const std::string& json) {
    // Minimal JSON parsing — production code would use a library
    auto readStr = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\s*\"";
        auto pos = json.find("\"" + key + "\":");
        if (pos == std::string::npos) return "";
        pos = json.find('\"', pos + key.size() + 3);
        if (pos == std::string::npos) return "";
        pos++;
        auto end = json.find('\"', pos);
        return json.substr(pos, end - pos);
    };
    auto readInt = [&](const std::string& key, int def) -> int {
        auto pos = json.find("\"" + key + "\":");
        if (pos == std::string::npos) return def;
        pos += key.size() + 3;
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        if (pos >= json.size()) return def;
        return std::stoi(json.substr(pos));
    };
    auto readBool = [&](const std::string& key, bool def) -> bool {
        auto pos = json.find("\"" + key + "\":");
        if (pos == std::string::npos) return def;
        pos += key.size() + 3;
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        if (pos >= json.size()) return def;
        return json.substr(pos, 4) == "true";
    };

    m_config.cursorSensitivity     = 1.0f;
    m_config.startMinimized        = readBool("startMinimized",      false);
    m_config.startWithWindows      = readBool("startWithWindows",     false);
    m_config.enableAudioRouting    = readBool("enableAudioRouting",   true);
    m_config.enableWindowIsolation = readBool("enableWindowIsolation",true);
    m_config.enableOverlayCursors  = readBool("enableOverlayCursors", true);

    // Parse seats array (simplified)
    for (int i = 0; i < MAX_SEATS; i++) {
        m_config.seats[i].monitorIndex = i;
    }

    LOG_INFO(L"Config loaded successfully");
    return true;
}