// AutoReflex - POEFixer Plugin (Phase 1: Bare Shell)
// Minimal implementation: exports, IPlugin interface, empty stubs

#define PLUGIN_EXPORTS
#include "AutoReflex.h"
#include "sdk/PluginHelpers.h"

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
// IPlugin implementation - Phase 1 bare shell
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
    // Phase 1: plugin loaded, nothing to initialize yet
}

void AutoReflexPlugin::OnDisable() {
    // Phase 1: plugin unloaded, nothing to clean up yet
}

void AutoReflexPlugin::DrawSettings() {
    // Phase 1: no settings UI yet
}

void AutoReflexPlugin::DrawUI() {
    // Phase 1: no overlay UI yet
}

void AutoReflexPlugin::SaveSettings() {
    // Phase 1: no settings persistence yet
}

// ============================================================================
// Unused stubs (called from headers to avoid linker errors)
// ============================================================================

void AutoReflexPlugin::Initialize() {
    // Phase 1: nothing to initialize
}

void AutoReflexPlugin::Tick(const std::shared_ptr<const PluginSDK::PluginGameSnapshot>& /*snapshot*/) {
    // Phase 1: no game logic yet
}

void AutoReflexPlugin::LoadSettings() {
    // Phase 1: no settings yet
}

void AutoReflexPlugin::DrawSettingsGeneral() {
    // Phase 1: stub
}

void AutoReflexPlugin::DrawSettingsRuleList() {
    // Phase 1: stub
}

void AutoReflexPlugin::DrawSettingsRuleEditor() {
    // Phase 1: stub
}

void AutoReflexPlugin::DrawSettingsScriptDocs() {
    // Phase 1: stub
}

void AutoReflexPlugin::DrawOverlayStatusWindow() {
    // Phase 1: stub
}