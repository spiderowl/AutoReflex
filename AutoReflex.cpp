// AutoReflex - POEFixer Plugin
// Phase 2: Read and Display Game Data
// Implements: WantsOverlay(), Debug UI, ShouldExecute integration, StatusMsg display

#include "AutoReflex.h"
#include "sdk/PluginHelpers.h"
#include "core/ShouldExecute.h"
#include "ui/EntityList.h"
#include "ui/EntityDetail.h"

#include <imgui.h>

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
    // Settings UI - Phase 6
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

    // --- Entity list using GetEntityDebugList (same as ExamplePlugin) ---------
    // GetEntityDebugList returns DebugEntityInfo with ComponentAddresses including
    // Buffs, which WatchEntity/GetWatchedEntityData can then populate.
    // RadarEntity from GetSnapshot does NOT have component addresses.
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