// AutoReflex - POEFixer Plugin
// Main plugin class implementing IPlugin

#pragma once

#include "sdk/PluginAPI.h"
#include "sdk/PluginContext.h"

#include <string>
#include <memory>
#include <chrono>

// Forward declarations
namespace AutoReflex {
    namespace Rules { class RuleManager; }
    namespace Storage { class RuleStore; class SettingsStore; }
    namespace Game { class ConditionState; }
    namespace Scripting { class ScriptEngine; class ScriptBindings; }
}

class AutoReflexPlugin : public IPlugin {
public:
    // --- IPlugin overrides ---
    void SetPluginDirectory(const char* dir) override;
    void SetContext(PluginContext* context) override;
    void OnEnable(bool isGameOpened) override;
    void OnDisable() override;
    void DrawSettings() override;
    void DrawUI() override;
    void SaveSettings() override;
    const char* GetName() override { return "AutoReflex"; }
    int GetSDKVersion() override { return PLUGIN_SDK_VERSION; }
    bool WantsOverlay() override { return m_OverlayEnabled; }

private:
    // --- Initialization ---
    void Initialize();

    // --- Tick: called every frame when overlay is visible ---
    void Tick(const std::shared_ptr<const PluginSDK::PluginGameSnapshot>& snapshot);

    // --- Settings persistence ---
    void LoadSettings();

    // --- Settings UI ---
    void DrawSettingsGeneral();
    void DrawSettingsRuleList();
    void DrawSettingsRuleEditor();
    void DrawSettingsScriptDocs();

    // --- Overlay UI ---
    void DrawOverlayStatusWindow();

    // --- Context helper ---
    PluginContext* Context() const { return m_Context; }

    // --- Members ---
    PluginContext* m_Context = nullptr;
    std::string m_Directory;

    // --- Settings ---
    bool m_OverlayEnabled = true;
    bool m_ShowStatusWindow = true;
    float m_WindowAlpha = 0.85f;
    int m_SimKeyMethod = 0;  // 0=SendInput, 1=SendKeyEvent, 2=RawKeyPress
    float m_GlobalCooldown = 0.5f;
    float m_KeyHoldDuration = 0.05f;

    // --- Subsystems ---
    std::unique_ptr<AutoReflex::Rules::RuleManager> m_RuleManager;
    std::unique_ptr<AutoReflex::Storage::RuleStore> m_RuleStore;
    std::unique_ptr<AutoReflex::Storage::SettingsStore> m_SettingsStore;
    std::unique_ptr<AutoReflex::Game::ConditionState> m_ConditionState;
    std::unique_ptr<AutoReflex::Scripting::ScriptEngine> m_ScriptEngine;
    std::unique_ptr<AutoReflex::Scripting::ScriptBindings> m_ScriptBindings;

    // --- State ---
    uint64_t m_LastAreaChangeCounter = 0;
    std::chrono::steady_clock::time_point m_GlobalCooldownExpiry;
    bool m_CooldownActive = false;
    int m_RulesFiredThisFrame = 0;

    // --- Settings UI state ---
    int m_SelectedRuleIndex = -1;
    bool m_ShowRuleEditor = false;
    int m_SettingsTab = 0;  // 0=General, 1=Rules, 2=Script Docs
};