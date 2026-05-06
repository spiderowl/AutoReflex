// AutoReflex - POEFixer Plugin
// Headless automation plugin: a settings UI for editing rules, plus a
// background tick (DrawUI) that evaluates rules and synthesizes key presses.
// There is intentionally no in-game overlay window.

#pragma once

#include "sdk/PluginAPI.h"
#include "sdk/PluginContext.h"

#include <string>
#include <memory>
#include <chrono>

#include "rules/RuleManager.h"
#include "storage/RuleStore.h"
#include "storage/SettingsStore.h"
#include "scripting/ScriptEngine.h"
#include "core/EvalThrottle.h"

namespace AutoReflex { namespace UI      { class SettingsPanel; } }
namespace AutoReflex { namespace Storage { class SettingsStore; } }

class AutoReflexPlugin : public IPlugin {
    friend class AutoReflex::UI::SettingsPanel;
    friend class AutoReflex::Storage::SettingsStore;

public:
    // --- IPlugin overrides ---
    void SetPluginDirectory(const char* dir) override;
    void SetContext(PluginContext* context) override;
    void OnEnable(bool isGameOpened) override;
    void OnDisable() override;
    void DrawSettings() override;
    void DrawUI() override;
    void SaveSettings() override;
    const char* GetName() override   { return "AutoReflex"; }
    int  GetSDKVersion() override    { return PLUGIN_SDK_VERSION; }
    // Always true: DrawUI is our background tick for rule evaluation, even
    // though it draws no visible window.
    bool WantsOverlay() override     { return true; }

private:
    void LoadSettings();

    // --- Host context ---
    PluginContext* m_Context = nullptr;
    std::string    m_Directory;

    // --- Persisted settings (config/settings.json) ---
    // Rule evaluation pacing. Default ~60 Hz so 144 Hz monitors don't pay
    // 144 Hz worth of host calls when 60 Hz is more than enough.
    int m_EvalIntervalMs = 16;

    // --- Subsystems ---
    ScriptEngine                                        m_ScriptEngine;
    std::unique_ptr<AutoReflex::Rules::RuleManager>     m_RuleManager;
    std::unique_ptr<AutoReflex::Storage::RuleStore>     m_RuleStore;
    std::unique_ptr<AutoReflex::Storage::SettingsStore> m_SettingsStore;
    AutoReflex::Core::EvalThrottle                      m_EvalThrottle;

    // --- "Test Fire" affordance in the Settings tab ---
    bool                                  m_TestFireEnabled     = false;
    float                                 m_TestFireCooldownSec = 0.8f;
    std::chrono::steady_clock::time_point m_LastTestFire;

    // --- UI selection state (transient, not persisted) ---
    int m_SelectedRuleIndex = -1;
};
