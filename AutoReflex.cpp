// AutoReflex - POEFixer Plugin
// Phase 3: Key Press and Cooldown
// Implements: PressKey(SendInput), Test fire cooldown, Debug log, Log keypresses

#include "AutoReflex.h"
#include "sdk/PluginHelpers.h"
#include "core/ShouldExecute.h"
#include "game/KeySender.h"
#include "ui/EntityList.h"
#include "ui/EntityDetail.h"

#include <imgui.h>
#include <sstream>
#include <iomanip>

using namespace PluginSDK;

// ============================================================================
// Factory exports (required by host)
// ============================================================================

extern "C" PLUGIN_API IPlugin* CreatePlugin() {
    return new AutoReflexPlugin();
}

extern "C" PLUGIN_API void DestroyPlugin(IPlugin* plugin) {
    delete plugin;
}

// ============================================================================
// IPlugin implementation
// ============================================================================

void AutoReflexPlugin::SetPluginDirectory(const char* dir) {
    m_Directory = dir;
}

void AutoReflexPlugin::SetContext(PluginContext* context) {
    m_Context = context;
    if (m_Context && m_Context->ImGuiContext) {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_Context->ImGuiContext));
    }
}

void AutoReflexPlugin::OnEnable(bool /*isGameOpened*/) {
    // Plugin enabled
}

void AutoReflexPlugin::OnDisable() {
    // Plugin disabled
}

void AutoReflexPlugin::DrawSettings() {
    // Always visible when plugin is enabled (even without game open)
    // Follows ExamplePlugin pattern: DrawSettings = configuration checkboxes

    // --- Test Fire (works without game) ---
    ImGui::Checkbox("Test Fire (Q)", &m_TestFireEnabled);
    ImGui::SameLine();
    ImGui::Text("Cooldown: %.1fms", m_TestFireCooldownSec * 1000.f);
    ImGui::SameLine();
    ImGui::SliderFloat("##TestCooldown", &m_TestFireCooldownSec, 0.1f, 3.0f, "%.1fs");

    if (m_TestFireEnabled) {
        auto now = std::chrono::steady_clock::now();
        if (now - m_LastTestFire >= std::chrono::milliseconds(static_cast<uint32_t>(m_TestFireCooldownSec * 1000.0f))) {
            AutoReflex::Game::PressKey('Q');
            m_LastTestFire = now;
            Log("Pressed Q — test fire");
        }
    }

    ImGui::Separator();

    // --- Panel visibility toggles ---
    ImGui::Text("Panels:");
    ImGui::Checkbox("Debug Log", &m_ShowDebugLog);
    ImGui::Checkbox("Show Entity List", &m_ShowEntityList);
    ImGui::Checkbox("Show Monster Detail", &m_ShowMonsterDetail);

    ImGui::Separator();

    // --- Overlay options ---
    ImGui::Checkbox("Enable Overlay", &m_OverlayEnabled);
    ImGui::SliderFloat("Window Opacity", &m_WindowAlpha, 0.3f, 1.0f, "%.1f");

    ImGui::Separator();

    // --- Debug Log in settings panel ---
    if (m_ShowDebugLog) {
        ImGui::Text("Log (%zu entries):", m_DebugLog.size());
        if (ImGui::SmallButton("Clear")) m_DebugLog.clear();
        ImGui::SameLine();
        ImVec2 logSize = ImGui::GetContentRegionAvail();
        logSize.y = 120.0f;
        ImGui::BeginChild("DebugLog", logSize, true);
        for (const auto& entry : m_DebugLog) {
            ImGui::TextUnformatted(entry.c_str());
        }
        ImGui::EndChild();
    }
}

// T14/T15/T16/T20: DrawUI with selectable monster list + detail panel
void AutoReflexPlugin::DrawUI() {
    if (!m_Context || !m_Context->IsAttached()) return;

    auto snapshot = m_Context->GetSnapshot();
    if (!snapshot) return;

    // T17/T18/T19: Evaluate ShouldExecute and store result
    std::string statusReason;
    bool canExecute = AutoReflex::ShouldExecute(m_Context, statusReason);

    // T20: Status color coding
    ImVec4 statusColor;
    if (canExecute) {
        statusColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);  // Green = active
    } else {
        if (statusReason == "In town" || statusReason == "In hideout" || statusReason == "Grace period") {
            statusColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow = warning
        } else {
            statusColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);  // Red = blocked
        }
    }

    // Store status message for external access
    m_StatusMsg = statusReason;

    ImGui::Begin("AutoReflex##AR", nullptr, ImGuiWindowFlags_NoCollapse);

    // T20: Display StatusMsg at top with color
    ImGui::TextColored(statusColor, "%s: %s", (canExecute ? "ACTIVE" : "BLOCKED"), statusReason.c_str());
    ImGui::Separator();

    // T14: Display basic game state
    ImGui::Text("State: %d", (int)snapshot->CurrentState);
    ImGui::Text("IsTown: %s", snapshot->IsTown ? "true" : "false");
    ImGui::Text("IsHideout: %s", snapshot->IsHideout ? "true" : "false");
    ImGui::Text("Foreground: %s", m_Context->IsGameForeground() ? "true" : "false");

    auto vitals = m_Context->GetPlayerVitals();
    ImGui::Text("HP%%: %d", vitals.HPPercent);
    ImGui::Separator();

    // --- T23: Debug log UI ---
    ImGui::Checkbox("Debug Log", &m_ShowDebugLog);
    if (m_ShowDebugLog) {
        ImGui::Text("Log (%zu entries):", m_DebugLog.size());
        if (ImGui::SmallButton("Clear")) m_DebugLog.clear();
        ImGui::SameLine();
        ImVec2 logSize = ImGui::GetContentRegionAvail();
        logSize.y = 120.0f;
        ImGui::BeginChild("DebugLog", logSize, true);
        for (const auto& entry : m_DebugLog) {
            ImGui::TextUnformatted(entry.c_str());
        }
        ImGui::EndChild();
    }

    ImGui::Separator();

    // --- Entity list using GetEntityDebugList (same as ExamplePlugin) ---------
    std::vector<PluginSDK::DebugEntityInfo> debugEntities;
    if (m_Context->GetEntityDebugList) {
        debugEntities = m_Context->GetEntityDebugList();
    }

    // Monster selection list (top panel) - delegated to EntityList module
    m_SelectedMonsterIdx = AutoReflex::DrawEntityList(debugEntities, m_SelectedMonsterIdx);

    ImGui::Separator();

    // Selected monster detail panel (bottom panel) - delegated to EntityDetail module
    ImGui::Text("Monster Details:");
    ImVec2 childSize = ImGui::GetContentRegionAvail();
    childSize.y -= ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("MonsterDetails", childSize, true);
    auto monsterIndices = AutoReflex::CollectMonsters(debugEntities);
    if (m_SelectedMonsterIdx >= 0 && m_SelectedMonsterIdx < (int)monsterIndices.size()) {
        const auto& entity = debugEntities[monsterIndices[m_SelectedMonsterIdx]];
        AutoReflex::DrawEntityDetail(m_Context, entity, m_WatchedEntityId, m_WatchFrameCounter);
    } else {
        ImGui::Text("Select a monster from the list above.");
    }
    ImGui::EndChild();

    ImGui::End();
}

// T23: Log helper - caps at DEBUG_LOG_MAX entries
void AutoReflexPlugin::Log(const std::string& msg)
{
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()) % 60;
    auto mins = std::chrono::duration_cast<std::chrono::minutes>(now.time_since_epoch()) % 60;

    std::ostringstream oss;
    oss << '['
        << std::setfill('0') << std::setw(2) << mins.count() << ':'
        << std::setfill('0') << std::setw(2) << secs.count() << '.'
        << std::setfill('0') << std::setw(3) << ms.count()
        << "] " << msg;

    m_DebugLog.push_back(oss.str());
    if (m_DebugLog.size() > DEBUG_LOG_MAX) {
        m_DebugLog.erase(m_DebugLog.begin());
    }
}

void AutoReflexPlugin::SaveSettings() {
    // Settings persistence - Phase 6
}

// ============================================================================
// Unused stubs (called from headers to avoid linker errors)
// ============================================================================

void AutoReflexPlugin::Initialize() {
}

void AutoReflexPlugin::Tick(const std::shared_ptr<const PluginSDK::PluginGameSnapshot>& /*snapshot*/) {
}

void AutoReflexPlugin::LoadSettings() {
}

void AutoReflexPlugin::DrawSettingsGeneral() {
}

void AutoReflexPlugin::DrawSettingsRuleList() {
}

void AutoReflexPlugin::DrawSettingsRuleEditor() {
}

void AutoReflexPlugin::DrawSettingsScriptDocs() {
}

void AutoReflexPlugin::DrawOverlayStatusWindow() {
}