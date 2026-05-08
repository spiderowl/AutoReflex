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

    /**
     * Loads persisted settings into the plugin instance.
     *
     * @returns None.
     */
    void LoadSettingsFromDisk();

    /**
     * Saves current plugin settings to disk.
     *
     * @returns None.
     */
    void SaveSettingsToDisk();

private:
    AutoReflexPlugin* m_Plugin;
    std::string    m_ConfigPath;
};

} // namespace Storage
} // namespace AutoReflex