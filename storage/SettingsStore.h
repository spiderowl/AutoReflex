// AutoReflex - SettingsStore
// Settings persistence to config/settings.json

#pragma once

#include <string>

struct PluginContext;

namespace AutoReflex {
namespace Storage {

class SettingsStore {
public:
    explicit SettingsStore(PluginContext* ctx);

    // Load settings from disk into PluginContext
    void Load();

    // Save settings from PluginContext to disk
    void Save();

private:
    PluginContext* m_Ctx;
    std::string    m_ConfigPath;
};

} // namespace Storage
} // namespace AutoReflex