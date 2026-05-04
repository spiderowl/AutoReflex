// AutoReflex - SettingsStore.cpp
// Persists plugin settings to config/settings.json
//
// Format (hand-rolled minimal JSON, no external deps):
// {
//   "OverlayEnabled": true,
//   "ShowStatusWindow": true,
//   "WindowAlpha": 0.85,
//   "SimKeyMethod": 0,
//   "GlobalCooldown": 0.5,
//   "KeyHoldDuration": 0.05,
//   "RunInHideout": false,
//   "RunInTown": false,
//   "OverlayX": 10,
//   "OverlayY": 10
// }

#include "SettingsStore.h"
#include "../AutoReflex.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace AutoReflex { namespace Storage {

// ── Minimal JSON helpers ────────────────────────────────────────────

static std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 10);
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string unquote(const std::string& s) {
    auto t = trim(s);
    if (t.size() >= 2 && t.front() == '\"' && t.back() == '\"')
        return t.substr(1, t.size() - 2);
    return t;
}

// Parse a simple JSON object (one level, values are bool/float/int/string)
static void parseJsonLine(const std::string& line, std::string& key, std::string& value) {
    // Expected: "key": value ,  or  "key": value
    size_t firstQuote = line.find('\"');
    if (firstQuote == std::string::npos) { key.clear(); value.clear(); return; }

    size_t secondQuote = line.find('\"', firstQuote + 1);
    if (secondQuote == std::string::npos) { key.clear(); value.clear(); return; }

    key = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);

    size_t colonPos = line.find(':', secondQuote);
    if (colonPos == std::string::npos) { value.clear(); return; }

    value = trim(line.substr(colonPos + 1));
    // Remove trailing comma
    if (!value.empty() && value.back() == ',') value.pop_back();
    value = trim(value);
}

static bool parseBool(const std::string& s) {
    return s == "true" || s == "1";
}

static float parseFloat(const std::string& s, float def) {
    try { return std::stof(s); }
    catch (...) { return def; }
}

static int parseInt(const std::string& s, int def) {
    try { return std::stoi(s); }
    catch (...) { return def; }
}

// ── SettingsStore implementation ────────────────────────────────────

SettingsStore::SettingsStore(AutoReflexPlugin* plugin)
    : m_Plugin(plugin)
{
    if (!m_Plugin) {
        m_ConfigPath = "config/settings.json";
        return;
    }

    // Always store config under the plugin directory so the host's CWD doesn't matter.
    std::filesystem::path p = std::filesystem::path(m_Plugin->m_Directory) / "config" / "settings.json";
    m_ConfigPath = p.string();
}

void SettingsStore::Load() {
    if (!m_Plugin) return;

    std::ifstream file(m_ConfigPath);
    if (!file.is_open()) return;

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    // Split by lines and parse
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        std::string key, value;
        parseJsonLine(line, key, value);
        if (key.empty()) continue;

        if (key == "OverlayEnabled") m_Plugin->m_OverlayEnabled = parseBool(value);
        else if (key == "ShowStatusWindow") m_Plugin->m_ShowStatusWindow = parseBool(value);
        else if (key == "WindowAlpha") m_Plugin->m_WindowAlpha = parseFloat(value, m_Plugin->m_WindowAlpha);
        else if (key == "SimKeyMethod") m_Plugin->m_SimKeyMethod = parseInt(value, m_Plugin->m_SimKeyMethod);
        else if (key == "GlobalCooldown") m_Plugin->m_GlobalCooldown = parseFloat(value, m_Plugin->m_GlobalCooldown);
        else if (key == "KeyHoldDuration") m_Plugin->m_KeyHoldDuration = parseFloat(value, m_Plugin->m_KeyHoldDuration);
        else if (key == "OverlayX") { /* reserved for future draggable overlay */ }
        else if (key == "OverlayY") { /* reserved for future draggable overlay */ }
        else if (key == "Profile") { (void)unquote(value); } // reserved
    }
}

void SettingsStore::Save() {
    if (!m_Plugin) return;

    // Ensure directory exists (config/)
    try {
        std::filesystem::path p(m_ConfigPath);
        std::filesystem::create_directories(p.parent_path());
    } catch (...) {
        // if directory creation fails, just attempt to write and let it fail
    }

    std::ofstream file(m_ConfigPath);
    if (!file.is_open()) return;

    file << "{\n";
    file << "  \"OverlayEnabled\": " << (m_Plugin->m_OverlayEnabled ? "true" : "false") << ",\n";
    file << "  \"ShowStatusWindow\": " << (m_Plugin->m_ShowStatusWindow ? "true" : "false") << ",\n";
    file << "  \"WindowAlpha\": " << m_Plugin->m_WindowAlpha << ",\n";
    file << "  \"SimKeyMethod\": " << m_Plugin->m_SimKeyMethod << ",\n";
    file << "  \"GlobalCooldown\": " << m_Plugin->m_GlobalCooldown << ",\n";
    file << "  \"KeyHoldDuration\": " << m_Plugin->m_KeyHoldDuration << "\n";
    file << "}\n";
}

} } // namespace AutoReflex::Storage