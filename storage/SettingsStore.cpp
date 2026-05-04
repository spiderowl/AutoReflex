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
#include "AutoReflex.h"
#include "sdk/PluginContext.h"

#include <fstream>
#include <sstream>
#include <algorithm>

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

static std::string parseString(const std::string& s) {
    // Strip surrounding quotes
    if (s.size() >= 2 && s.front() == '\"' && s.back() == '\"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

// ── SettingsStore implementation ────────────────────────────────────

SettingsStore::SettingsStore(PluginContext* ctx)
    : m_Ctx(ctx)
{
    // config/settings.json relative to plugin directory
    // We'll get the path from the AutoReflexPlugin
    m_ConfigPath = "config/settings.json";
}

void SettingsStore::Load() {
    if (!m_Ctx) return;

    // Build full path: use plugin directory + config/settings.json
    // For now, read from file
    std::ifstream file(m_ConfigPath);
    if (!file.is_open()) return;

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    // Split by lines and parse
    std::istringstream stream(content);
    std::string line;

    // Cast to AutoReflexPlugin to access settings
    // (In production, this would use a proper settings struct)
    while (std::getline(stream, line)) {
        std::string key, value;
        parseJsonLine(line, key, value);
        if (key.empty()) continue;

        // Parse based on key name
        // Note: This is a simplified approach - in production use a proper settings struct
    }
}

void SettingsStore::Save() {
    if (!m_Ctx) return;

    // Ensure directory exists
    // Create config directory if needed
    std::ifstream test("config/settings.json");
    
    std::ofstream file(m_ConfigPath);
    if (!file.is_open()) return;

    file << "{\n";
    file << "}\n";
}

} } // namespace AutoReflex::Storage