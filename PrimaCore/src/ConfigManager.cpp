// ============================================================
// Prima Multi Seat — ConfigManager.cpp
// Enterprise-Grade Production Configuration Module
// Target: MSVC C++17, Zero Warnings, No External Dependencies
// ============================================================
#include "../include/ConfigManager.h"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <cassert>
#include <cstring>
// ============================================================
// Internal constants — never exposed to callers
// ============================================================
namespace {
    constexpr float   kDefaultSensitivity = 1.0f;
    constexpr float   kMinSensitivity     = 0.1f;
    constexpr float   kMaxSensitivity     = 10.0f;
    constexpr size_t  kMaxJsonSize        = 1u * 1024u * 1024u; // 1 MB cap
    constexpr size_t  kMaxStringFieldLen  = 512u;
    constexpr int     kWinApiError        = 0;
    constexpr DWORD   kDefaultCursorColor = 0xFFFFFFFFu;
    template<typename T>
    T SafeClamp(T val, T lo, T hi) noexcept {
        return (val < lo) ? lo : (val > hi) ? hi : val;
    }
} // anonymous namespace
// ============================================================
// Lifecycle
// ============================================================
ConfigManager::ConfigManager() noexcept {
    SetDefaults();
}
ConfigManager::~ConfigManager() noexcept = default;
void ConfigManager::SetDefaults() noexcept {
    m_config = PrimaConfig{};  // Value-initialise — zero all POD fields
    m_config.cursorSensitivity     = kDefaultSensitivity;
    m_config.startMinimized        = false;
    m_config.startWithWindows      = false;
    m_config.enableAudioRouting    = true;
    m_config.enableWindowIsolation = true;
    m_config.enableOverlayCursors  = true;
    static_assert(MAX_SEATS > 0, "MAX_SEATS must be positive");
    for (int i = 0; i < MAX_SEATS; ++i) {
        m_config.seats[i]              = SeatConfig{};
        m_config.seats[i].monitorIndex = i;
        m_config.seats[i].cursorColor  = kDefaultCursorColor;
    }
}
// ============================================================
// Public Load — always returns valid state; never throws
// ============================================================
bool ConfigManager::Load(const std::wstring& path) noexcept {
    if (path.empty()) {
        LOG_WARN(L"ConfigManager::Load — empty path, using defaults");
        SetDefaults();
        return false;
    }
    m_path = path;
    std::string json;
    if (!ReadFileContents(path, json)) {
        LOG_WARN(L"ConfigManager::Load — file unreadable, using defaults");
        SetDefaults();
        return false;
    }
    if (json.empty()) {
        LOG_WARN(L"ConfigManager::Load — empty file, using defaults");
        SetDefaults();
        return false;
    }
    SetDefaults();  // Clean baseline — deterministic across repeated calls
    const bool ok = ParseJSON(json);
    if (!ok) LOG_WARN(L"ConfigManager::Load — parse failed, retaining defaults");
    SanitiseConfig();
    LOG_INFO(L"ConfigManager::Load — completed (ok=" + std::to_wstring(ok) + L")");
    return ok;
}
// ============================================================
// Public Save — atomic write: temp file + MoveFileExW rename
// ============================================================
bool ConfigManager::Save(const std::wstring& path) const noexcept {
    if (path.empty()) {
        LOG_ERROR(L"ConfigManager::Save — empty path");
        return false;
    }
    std::string payload;
    try { payload = BuildJSON(); }
    catch (...) {
        LOG_ERROR(L"ConfigManager::Save — BuildJSON threw unexpectedly");
        return false;
    }
    if (payload.empty()) {
        LOG_ERROR(L"ConfigManager::Save — empty payload");
        return false;
    }
    const std::wstring tmpPath = path + L".tmp";
    {
        std::ofstream f(tmpPath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!f.is_open()) {
            LOG_ERROR(L"ConfigManager::Save — cannot open temp file");
            return false;
        }
        f.write(payload.c_str(), static_cast<std::streamsize>(payload.size()));
        if (f.fail()) {
            f.close(); ::DeleteFileW(tmpPath.c_str());
            LOG_ERROR(L"ConfigManager::Save — write failed"); return false;
        }
        f.flush();
        if (f.fail()) {
            f.close(); ::DeleteFileW(tmpPath.c_str());
            LOG_ERROR(L"ConfigManager::Save — flush failed"); return false;
        }
    }
    if (!::MoveFileExW(tmpPath.c_str(), path.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        LOG_ERROR(L"ConfigManager::Save — MoveFileExW failed, error=" +
                  std::to_wstring(::GetLastError()));
        ::DeleteFileW(tmpPath.c_str());
        return false;
    }
    LOG_INFO(L"ConfigManager::Save — written: " + path);
    return true;
}
// ============================================================
// File I/O — reads entire file, enforces 1 MB size cap
// ============================================================
bool ConfigManager::ReadFileContents(const std::wstring& path,
                                     std::string& out) const noexcept {
    out.clear();
    try {
        std::ifstream f(path, std::ios::in | std::ios::binary);
        if (!f.is_open()) return false;
        f.seekg(0, std::ios::end);
        const auto rawSize = f.tellg();
        f.seekg(0, std::ios::beg);
        if (rawSize <= 0) return false;
        const auto fileSize = static_cast<size_t>(rawSize);
        if (fileSize > kMaxJsonSize) {
            LOG_ERROR(L"ReadFileContents — file exceeds 1 MB cap");
            return false;
        }
        out.resize(fileSize);
        f.read(out.data(), static_cast<std::streamsize>(fileSize));
        const auto bytesRead = static_cast<size_t>(f.gcount());
        if (bytesRead != fileSize) out.resize(bytesRead); // partial read handled
        return !out.empty();
    } catch (const std::bad_alloc&) {
        LOG_ERROR(L"ReadFileContents — out of memory"); out.clear(); return false;
    } catch (...) {
        LOG_ERROR(L"ReadFileContents — unexpected exception"); out.clear(); return false;
    }
}
// ============================================================
// WinAPI UTF-8 Conversion — fully validated, no underflow
// ============================================================
std::string ConfigManager::WStringToUTF8(const std::wstring& ws) const noexcept {
    if (ws.empty()) return {};
    const int required = ::WideCharToMultiByte(
        CP_UTF8, 0,
        ws.c_str(), static_cast<int>(ws.size()), // explicit len — no null ambiguity
        nullptr, 0, nullptr, nullptr);
    if (required <= kWinApiError) {
        LOG_ERROR(L"WStringToUTF8 — size query failed"); return {};
    }
    std::string result;
    try { result.resize(static_cast<size_t>(required)); } catch (...) { return {}; }
    const int written = ::WideCharToMultiByte(
        CP_UTF8, 0,
        ws.c_str(), static_cast<int>(ws.size()),
        result.data(), required, nullptr, nullptr);
    if (written <= kWinApiError) {
        LOG_ERROR(L"WStringToUTF8 — conversion failed"); return {};
    }
    result.resize(static_cast<size_t>(written));
    return result;
}
std::wstring ConfigManager::UTF8ToWString(const std::string& s) const noexcept {
    if (s.empty()) return {};
    const int required = ::MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, // strict: reject invalid sequences
        s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (required <= kWinApiError) {
        // Lenient retry — tolerate legacy configs with non-strict UTF-8
        const int lenient = ::MultiByteToWideChar(
            CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
        if (lenient <= kWinApiError) return {};
        std::wstring ws;
        try { ws.resize(static_cast<size_t>(lenient)); } catch (...) { return {}; }
        ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                              ws.data(), lenient);
        return ws;
    }
    std::wstring result;
    try { result.resize(static_cast<size_t>(required)); } catch (...) { return {}; }
    const int written = ::MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        s.c_str(), static_cast<int>(s.size()), result.data(), required);
    if (written <= kWinApiError) return {};
    result.resize(static_cast<size_t>(written));
    return result;
}
// ============================================================
// Escape — full RFC 8259 control-character coverage
// ============================================================
std::string ConfigManager::Escape(const std::string& s) const noexcept {
    std::string out;
    try {
        out.reserve(s.size() + 16);
        for (const unsigned char c : s) {
            switch (c) {
                case '"':  out += "\\\"";  break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                case '\b': out += "\\b";  break;
                case '\f': out += "\\f";  break;
                default:
                    if (c < 0x20u) {
                        char buf[8];
                        (void)::snprintf(buf, sizeof(buf), "\\u%04X", (unsigned)c);
                        out += buf;
                    } else { out += static_cast<char>(c); }
                    break;
            }
        }
    } catch (...) { /* return partial on OOM */ }
    return out;
}
// ============================================================
// BuildJSON — deterministic, locale-safe, valid UTF-8 output
// ============================================================
std::string ConfigManager::BuildJSON() const {
    const PrimaConfig& cfg = m_config;
    std::ostringstream o;
    o.exceptions(std::ios::failbit | std::ios::badbit);
    o.imbue(std::locale::classic()); // locale-independent decimal point
    o << std::fixed << std::setprecision(6);
    o << "{\n";
    o << "  \"version\": \"" << PRIMA_VERSION_MAJOR << '.' << PRIMA_VERSION_MINOR
      << '.' << PRIMA_VERSION_PATCH << "\",\n";
    o << "  \"general\": {\n";
    o << "    \"cursorSensitivity\": "
      << SafeClamp(cfg.cursorSensitivity, kMinSensitivity, kMaxSensitivity) << ",\n";
    o << "    \"startMinimized\": "       << (cfg.startMinimized        ? "true":"false") << ",\n";
    o << "    \"startWithWindows\": "     << (cfg.startWithWindows      ? "true":"false") << ",\n";
    o << "    \"enableAudioRouting\": "   << (cfg.enableAudioRouting    ? "true":"false") << ",\n";
    o << "    \"enableWindowIsolation\": "<< (cfg.enableWindowIsolation ? "true":"false") << ",\n";
    o << "    \"enableOverlayCursors\": " << (cfg.enableOverlayCursors  ? "true":"false") << "\n";
    o << "  },\n";
    o << "  \"seats\": [\n";
    for (int i = 0; i < MAX_SEATS; ++i) {
        const SeatConfig& seat = cfg.seats[i];
        o << "    {\n";
        o << "      \"seatIndex\": "     << i                                           << ",\n";
        o << "      \"keyboardPath\": \"" << Escape(WStringToUTF8(seat.keyboardPath))  << "\",\n";
        o << "      \"mousePath\": \""    << Escape(WStringToUTF8(seat.mousePath))     << "\",\n";
        o << "      \"monitorIndex\": "  << seat.monitorIndex                           << ",\n";
        o << "      \"audioDeviceId\": \""<< Escape(WStringToUTF8(seat.audioDeviceId)) << "\",\n";
        o << "      \"cursorColor\": "   << static_cast<unsigned long>(seat.cursorColor)<< "\n";
        o << "    }";
        if (i < MAX_SEATS - 1) o << ',';
        o << "\n";
    }
    o << "  ]\n";
    o << "}\n";
    return o.str();
}
// ============================================================
// ParseJSON — fault-tolerant, never crashes on bad input
// ============================================================
bool ConfigManager::ParseJSON(const std::string& json) noexcept {
    auto SafeReadString = [&](const std::string& key,
                              const std::string& src) -> std::string {
        try {
            const std::string needle = "\"" + key + "\":";
            const size_t keyPos = src.find(needle);
            if (keyPos == std::string::npos) return {};
            const size_t openQ = src.find('"', keyPos + needle.size());
            if (openQ == std::string::npos) return {};
            size_t cur = openQ + 1;
            std::string val; val.reserve(64);
            while (cur < src.size()) {
                const char c = src[cur];
                if (c == '"') break;
                if (c == '\\' && cur + 1 < src.size()) {
                    const char n = src[++cur];
                    switch (n) {
                        case '"': val+='"'; break; case '\\': val+='\\'; break;
                        case 'n': val+='\n'; break; case 'r': val+='\r'; break;
                        case 't': val+='\t'; break; default: val+=n; break;
                    }
                    ++cur; continue;
                }
                val += c;
                if (val.size() >= kMaxStringFieldLen) break;
                ++cur;
            }
            return val;
        } catch (...) { return {}; }
    };
    auto SafeReadBool = [&](const std::string& key, bool def) -> bool {
        try {
            const std::string needle = "\"" + key + "\":";
            size_t pos = json.find(needle);
            if (pos == std::string::npos) return def;
            pos += needle.size();
            while (pos < json.size() &&
                   (json[pos]==' '||json[pos]=='\t'||json[pos]=='\r'||json[pos]=='\n'))
                ++pos;
            if (pos + 4 > json.size()) return def;  // bounds guard — no substr throw
            if (json.compare(pos, 4, "true")  == 0) return true;
            if (json.compare(pos, 5, "false") == 0) return false;
            return def;
        } catch (...) { return def; }
    };
    auto SafeReadInt = [&](const std::string& key,
                           const std::string& src, int def) -> int {
        try {
            const std::string needle = "\"" + key + "\":";
            size_t pos = src.find(needle);
            if (pos == std::string::npos) return def;
            pos += needle.size();
            while (pos < src.size() && (src[pos]==' '||src[pos]=='\t')) ++pos;
            if (pos >= src.size()) return def;
            const size_t ns = pos;
            if (src[pos] == '-') ++pos;
            while (pos < src.size() && std::isdigit((unsigned char)src[pos])) ++pos;
            if (pos == ns) return def;
            const std::string num = src.substr(ns, pos - ns);
            if (num.size() > 10) return def;  // reject absurd values pre-parse
            return std::stoi(num);
        } catch (const std::invalid_argument&) { return def; }
          catch (const std::out_of_range&)     { return def; }
          catch (...)                           { return def; }
    };
    auto SafeReadFloat = [&](const std::string& key, float def) -> float {
        try {
            const std::string needle = "\"" + key + "\":";
            size_t pos = json.find(needle);
            if (pos == std::string::npos) return def;
            pos += needle.size();
            while (pos < json.size() && (json[pos]==' '||json[pos]=='\t')) ++pos;
            const size_t ns = pos;
            if (pos < json.size() && json[pos]=='-') ++pos;
            while (pos < json.size() &&
                   (std::isdigit((unsigned char)json[pos]) ||
                    json[pos]=='.' || json[pos]=='e' || json[pos]=='E')) ++pos;
            if (pos == ns) return def;
            const std::string num = json.substr(ns, pos - ns);
            if (num.size() > 32) return def;
            const float val = std::stof(num);
            if (!std::isfinite(val)) return def; // reject NaN / Inf
            return val;
        } catch (...) { return def; }
    };
    // --- Parse general section ---
    m_config.cursorSensitivity     = SafeReadFloat("cursorSensitivity", kDefaultSensitivity);
    m_config.startMinimized        = SafeReadBool("startMinimized",       false);
    m_config.startWithWindows      = SafeReadBool("startWithWindows",      false);
    m_config.enableAudioRouting    = SafeReadBool("enableAudioRouting",    true);
    m_config.enableWindowIsolation = SafeReadBool("enableWindowIsolation", true);
    m_config.enableOverlayCursors  = SafeReadBool("enableOverlayCursors",  true);
    // --- Parse seats array (depth-aware brace matching) ---
    const std::string seatsNeedle = "\"seats\":";
    size_t seatsPos = json.find(seatsNeedle);
    if (seatsPos != std::string::npos)
        seatsPos = json.find('[', seatsPos + seatsNeedle.size());
    if (seatsPos != std::string::npos) {
        int parsedSeats = 0;
        size_t cur = seatsPos + 1;
        while (cur < json.size() && parsedSeats < MAX_SEATS) {
            size_t objStart = json.find('{', cur);
            if (objStart == std::string::npos) break;
            // Depth-aware close-brace scan
            size_t objEnd = std::string::npos;
            int depth = 0; bool inStr = false, esc = false;
            for (size_t i = objStart; i < json.size(); ++i) {
                const char c = json[i];
                if (esc) { esc=false; continue; }
                if (c=='\\' && inStr) { esc=true; continue; }
                if (c=='"') { inStr=!inStr; continue; }
                if (inStr) continue;
                if (c=='{') ++depth;
                if (c=='}') { if (--depth==0) { objEnd=i; break; } }
            }
            if (objEnd == std::string::npos) break; // malformed — stop safely
            const std::string obj = json.substr(objStart, objEnd - objStart + 1);
            const int rawIdx  = SafeReadInt("seatIndex", obj, parsedSeats);
            const int seatIdx = SafeClamp(rawIdx, 0, MAX_SEATS - 1);
            SeatConfig& seat  = m_config.seats[seatIdx];
            seat.keyboardPath = UTF8ToWString(SafeReadString("keyboardPath", obj));
            seat.mousePath    = UTF8ToWString(SafeReadString("mousePath",    obj));
            seat.audioDeviceId= UTF8ToWString(SafeReadString("audioDeviceId",obj));
            seat.monitorIndex = SafeReadInt("monitorIndex", obj, seatIdx);
            seat.cursorColor  = static_cast<COLORREF>(
                static_cast<unsigned long>(
                    SafeReadInt("cursorColor", obj,
                                static_cast<int>(kDefaultCursorColor))));
            ++parsedSeats;
            cur = objEnd + 1;
        }
    }
    return true; // partial success is success — defaults fill any gaps
}
// ============================================================
// SanitiseConfig — post-parse integrity clamp pass
// ============================================================
void ConfigManager::SanitiseConfig() noexcept {
    m_config.cursorSensitivity =
        SafeClamp(m_config.cursorSensitivity, kMinSensitivity, kMaxSensitivity);
    for (int i = 0; i < MAX_SEATS; ++i) {
        SeatConfig& seat = m_config.seats[i];
        if (seat.monitorIndex < 0) seat.monitorIndex = i;
        if (seat.keyboardPath.size()  > kMaxStringFieldLen) seat.keyboardPath.resize(kMaxStringFieldLen);
        if (seat.mousePath.size()     > kMaxStringFieldLen) seat.mousePath.resize(kMaxStringFieldLen);
        if (seat.audioDeviceId.size() > kMaxStringFieldLen) seat.audioDeviceId.resize(kMaxStringFieldLen);
    }
}
