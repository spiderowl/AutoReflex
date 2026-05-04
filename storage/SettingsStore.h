// AutoReflex - SettingsStore
// Settings persistence to config/settings.json

#pragma once

#include <string>

class AutoReflexPlugin;

namespace AutoReflex {
namespace Storage {

class SettingsStore {
public:
    explicit SettingsStore(AutoReflexPlugin* plugin);

    // Load settings from disk into plugin members
    void Load();

    // Save settings from plugin members to disk
    void Save();

private:
    AutoReflexPlugin* m_Plugin;
    std::string    m_ConfigPath;
};

} // namespace Storage
} // namespace AutoReflex