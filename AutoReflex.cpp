// AutoReflex - POEFixer Plugin
// Lifecycle + DrawUI orchestration.
//
// DrawUI() runs at host render rate. It draws nothing — we use it purely as
// a background tick:
//   1) fetch snapshot once
//   2) check ShouldExecute() against that snapshot
//   3) on the throttled cadence (~60 Hz by default), evaluate rules and
//      synthesize key presses for the rules that fired

#include "AutoReflex.h"
#include "sdk/PluginHelpers.h"
#include "core/ShouldExecute.h"
#include "game/KeySender.h"
#include "rules/RuleManager.h"
#include "storage/RuleStore.h"
#include "storage/SettingsStore.h"
#include "scripting/ScriptEngine.h"
#include "ui/SettingsPanel.h"

#include <imgui.h>
#include <filesystem>

using namespace PluginSDK;

extern "C" PLUGIN_API IPlugin* CreatePlugin() {
    return new AutoReflexPlugin();
}

extern "C" PLUGIN_API void DestroyPlugin(IPlugin* plugin) {
    delete plugin;
}

void AutoReflexPlugin::SetPluginDirectory(const char* pluginDirectoryPath) {
    m_Directory = pluginDirectoryPath ? pluginDirectoryPath : "";
    try {
        const std::filesystem::path pluginConfigDirectoryPath =
            std::filesystem::path(m_Directory) / "config";
        std::filesystem::create_directories(pluginConfigDirectoryPath);

        ScriptEngine::SetBuffsDumpPath(
            (pluginConfigDirectoryPath / "AutoReflex_BuffsDump.txt").string());
        ScriptEngine::SetBuffsDumpEnabled(
            std::filesystem::exists(pluginConfigDirectoryPath / "enable_buffs_dump.txt"));
    } catch (...) {
    }
}

void AutoReflexPlugin::SetContext(PluginContext* pluginContext) {
    m_Context = pluginContext;
    if (m_Context && m_Context->ImGuiContext) {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_Context->ImGuiContext));
    }
}

void AutoReflexPlugin::OnEnable(bool /*isGameOpened*/) {
    if (!m_ScriptEngine.HasInitializedScriptEngineSubsystem()) {
        m_ScriptEngine.InitializeScriptEngineSubsystem();
    }

    try {
        const std::filesystem::path pluginConfigDirectoryPath =
            std::filesystem::path(m_Directory) / "config";
        std::filesystem::create_directories(pluginConfigDirectoryPath);

        ScriptEngine::SetBuffsDumpPath(
            (pluginConfigDirectoryPath / "AutoReflex_BuffsDump.txt").string());
        ScriptEngine::SetBuffsDumpEnabled(
            std::filesystem::exists(pluginConfigDirectoryPath / "enable_buffs_dump.txt"));
    } catch (...) {
    }

    if (!m_RuleManager) {
        m_RuleManager = std::make_unique<AutoReflex::Rules::RuleManager>();
    }

    if (!m_RuleStore) {
        m_RuleStore = std::make_unique<AutoReflex::Storage::RuleStore>(m_Directory + "/rules");
    }

    if (!m_SettingsStore) {
        m_SettingsStore = std::make_unique<AutoReflex::Storage::SettingsStore>(this);
    }

    LoadSettings();
}

void AutoReflexPlugin::OnDisable() {
    SaveSettings();
}

void AutoReflexPlugin::DrawSettings() {
    AutoReflex::UI::SettingsPanel::Draw(this);

    if (m_TestFireEnabled) {
        const auto now = std::chrono::steady_clock::now();
        if (now - m_LastTestFire >= std::chrono::milliseconds(static_cast<uint32_t>(m_TestFireCooldownSec * 1000.0f))) {
            AutoReflex::Game::PressKey('Q');
            m_LastTestFire = now;
        }
    }
}

void AutoReflexPlugin::DrawUI() {
    if (!m_Context || !m_Context->IsAttached || !m_Context->IsAttached()) return;

    // Single snapshot per tick — avoids the 3 separate GetSnapshot() calls
    // the previous version did (DrawUI, ShouldExecute, EvaluateAll).
    auto snapshot = m_Context->GetSnapshot ? m_Context->GetSnapshot() : nullptr;
    if (!snapshot) return;

    std::string executionGateReason;
    if (!AutoReflex::DetermineWhetherRulesShouldExecute(
            m_Context, snapshot.get(), executionGateReason)) {
        return;
    }

    // Throttle rule evaluation to ~60 Hz by default. Even on a 144 Hz monitor,
    // rules don't need 144 Hz cadence (cooldowns are 100s of ms).
    if (m_RuleManager &&
        m_EvalThrottle.DetermineWhetherEvaluationShouldRunNow(m_EvalIntervalMs)) {
        m_RuleManager->EvaluateRulesAgainstSnapshotUntilFirstFire(m_Context, snapshot.get(),
            [](const AutoReflex::Rules::Rule& rule) {
                if (rule.Key > 0) {
                    AutoReflex::Game::PressKey(static_cast<WORD>(rule.Key));
                }
            });
    }
}

void AutoReflexPlugin::SaveSettings() {
    if (m_SettingsStore) m_SettingsStore->SaveSettingsToDisk();

    if (m_RuleStore && m_RuleManager) {
        for (auto& rule : m_RuleManager->GetRules()) {
            m_RuleStore->SaveRule(rule);
        }
    }
}

void AutoReflexPlugin::LoadSettings() {
    if (m_SettingsStore) m_SettingsStore->LoadSettingsFromDisk();

    if (m_RuleStore && m_RuleManager) {
        m_RuleManager->LoadAndCompileRulesFromStore(*m_RuleStore);
    }
}
