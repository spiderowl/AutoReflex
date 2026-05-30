// AutoReflex - POEFixer Plugin
// Lifecycle + DrawUI orchestration.

#include "AutoReflex.h"
#include "core/ShouldExecute.h"
#include "game/KeySender.h"
#include "rules/RuleManager.h"
#include "storage/RuleStore.h"
#include "storage/SettingsStore.h"
#include "scripting/ScriptEngine.h"
#include "ui/SettingsPanel.h"

#include <imgui.h>
#include <filesystem>

void AutoReflexPlugin::ConfigureBuffsDebugDumpPaths() {
    try {
        const std::filesystem::path pluginConfigDirectoryPath = DirectoryPath() / "config";
        std::filesystem::create_directories(pluginConfigDirectoryPath);

        ScriptEngine::SetBuffsDumpPath(
            (pluginConfigDirectoryPath / "AutoReflex_BuffsDump.txt").string());
        ScriptEngine::SetBuffsDumpEnabled(
            std::filesystem::exists(pluginConfigDirectoryPath / "enable_buffs_dump.txt"));
    } catch (...) {
    }
}

void AutoReflexPlugin::SubscribeToHostEvents() {
    if (!ctx()) return;

    auto& events = const_cast<PluginSDK::EventsService&>(ctx()->Events);

    m_AreaChangeToken = events.OnAreaChange([this]() {
        m_AnimationLock.Reset();
        m_LastExecutionGateReason = "Area change";
    });

    m_GameDetachedToken = events.OnGameDetached([this]() {
        SaveSettings();
        m_LastExecutionGateReason = "Game detached";
    });
}

void AutoReflexPlugin::UnsubscribeFromHostEvents() {
    if (!ctx()) return;
    auto& events = const_cast<PluginSDK::EventsService&>(ctx()->Events);
    if (m_AreaChangeToken.Valid()) events.Unsubscribe(m_AreaChangeToken);
    if (m_GameDetachedToken.Valid()) events.Unsubscribe(m_GameDetachedToken);
    m_AreaChangeToken = {};
    m_GameDetachedToken = {};
}

void AutoReflexPlugin::OnEnable(bool /*isGameAttached*/) {
    if (!HostCompatible() || !ctx()) return;

    if (ctx()->ImGuiContext) {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
    }

    ConfigureBuffsDebugDumpPaths();
    SubscribeToHostEvents();

    if (!m_RuleManager) {
        m_RuleManager = std::make_unique<AutoReflex::Rules::RuleManager>();
    }

    if (!m_RuleStore) {
        m_RuleStore = std::make_unique<AutoReflex::Storage::RuleStore>(
            (DirectoryPath() / "rules").string());
    }

    if (!m_SettingsStore) {
        m_SettingsStore = std::make_unique<AutoReflex::Storage::SettingsStore>(this);
    }

    LoadSettings();
}

void AutoReflexPlugin::OnDisable() {
    UnsubscribeFromHostEvents();
    SaveSettings();
}

void AutoReflexPlugin::DrawSettings() {
    AutoReflex::UI::SettingsPanel::Draw(this);

    if (m_TestFireEnabled) {
        const auto now = std::chrono::steady_clock::now();
        if (now - m_LastTestFire >= std::chrono::milliseconds(static_cast<uint32_t>(m_TestFireCooldownSec * 1000.0f))) {
            WORD testKey = 'Q';
            if (m_RuleManager && m_SelectedRuleIndex >= 0 &&
                m_SelectedRuleIndex < static_cast<int>(m_RuleManager->GetRules().size())) {
                const auto& selectedRule = m_RuleManager->GetRules()[static_cast<size_t>(m_SelectedRuleIndex)];
                if (selectedRule.Key > 0) {
                    testKey = static_cast<WORD>(selectedRule.Key);
                }
            }
            AutoReflex::Game::PressKey(testKey);
            m_LastTestFire = now;
        }
    }
}

void AutoReflexPlugin::DrawUI() {
    if (!HostCompatible() || !ctx()) {
        m_LastExecutionGateReason = "SDK incompatible";
        return;
    }

    const PluginSDK::Context& hostContext = *ctx();

    if (!hostContext.Game.IsAttached()) {
        m_LastExecutionGateReason = "Not attached";
        return;
    }
    if (!hostContext.Game.IsInGame()) {
        m_LastExecutionGateReason = "Not in game";
        return;
    }
    if (!hostContext.Game.IsForeground()) {
        m_LastExecutionGateReason = "Game not foreground";
        return;
    }

    if (m_AnimationLock.IsLocked()) {
        m_LastExecutionGateReason = "Animation wait";
        return;
    }

    const PluginSDK::Snapshot snapshot = hostContext.Game.GetSnapshot();

    AutoReflex::Core::EvalTickCache tickCache;
    tickCache.BeginTick(hostContext, snapshot);

    std::string executionGateReason;
    if (!AutoReflex::DetermineWhetherRulesShouldExecute(
            snapshot, tickCache, executionGateReason)) {
        m_LastExecutionGateReason = std::move(executionGateReason);
        return;
    }

    if (m_RuleManager) {
        m_RuleManager->EvaluateRulesAgainstSnapshotUntilFirstFire(
            hostContext,
            snapshot,
            tickCache,
            m_AnimationLock,
            [](const AutoReflex::Rules::Rule& rule) {
                if (rule.Key > 0) {
                    AutoReflex::Game::PressKey(static_cast<WORD>(rule.Key));
                }
            });
    }

    m_LastExecutionGateReason = "Active";
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

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() {
    return new AutoReflexPlugin();
}

extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* plugin) {
    delete plugin;
}
