// AutoReflex - SettingsStore.cpp
// Persists plugin settings to <pluginDir>/config/settings.json
//
// Format (hand-rolled minimal JSON, no external deps):
// {
//   "EvalIntervalMs": 16
// }

#include "SettingsStore.h"
#include "../AutoReflex.h"

#include <fstream>
#include <sstream>
#include <filesystem>

namespace AutoReflex { namespace Storage {

// ── Minimal JSON helpers ────────────────────────────────────────────

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Parse a simple JSON object line: "key": value (with optional trailing comma).
static void parseJsonLine(const std::string& line, std::string& key, std::string& value) {
    size_t firstQuote = line.find('\"');
    if (firstQuote == std::string::npos) { key.clear(); value.clear(); return; }

    size_t secondQuote = line.find('\"', firstQuote + 1);
    if (secondQuote == std::string::npos) { key.clear(); value.clear(); return; }

    key = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);

    size_t colonPos = line.find(':', secondQuote);
    if (colonPos == std::string::npos) { value.clear(); return; }

    value = trim(line.substr(colonPos + 1));
    if (!value.empty() && value.back() == ',') value.pop_back();
    value = trim(value);
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

void SettingsStore::LoadSettingsFromDisk() {
    if (!m_Plugin) return;

    std::ifstream file(m_ConfigPath);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        std::string key, value;
        parseJsonLine(line, key, value);
        if (key.empty()) continue;

        if (key == "EvalIntervalMs") m_Plugin->m_EvalIntervalMs = parseInt(value, m_Plugin->m_EvalIntervalMs);
        // Unknown keys (including legacy ones from older builds) are silently ignored.
    }
}

void SettingsStore::SaveSettingsToDisk() {
    if (!m_Plugin) return;

    try {
        std::filesystem::path p(m_ConfigPath);
        std::filesystem::create_directories(p.parent_path());
    } catch (...) {
        // if directory creation fails, just attempt to write and let it fail
    }

    std::ofstream file(m_ConfigPath);
    if (!file.is_open()) return;

    file << "{\n"
         << "  \"EvalIntervalMs\": " << m_Plugin->m_EvalIntervalMs << "\n"
         << "}\n";
}

} } // namespace AutoReflex::Storage
