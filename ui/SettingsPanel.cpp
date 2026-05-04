/*  */// AutoReflex - SettingsPanel.cpp
// Full settings panel with General, Rules, Rule Editor, Script Docs tabs

#include "SettingsPanel.h"
#include "../AutoReflex.h"
#include "../rules/RuleManager.h"
#include "../storage/RuleStore.h"

#include <imgui.h>
#include <sstream>
#include <algorithm>
#include <chrono>

namespace AutoReflex {
namespace UI {

// ── Key name helper ──
static const char* KeyToString(uint16_t key) {
    if (key == 0) return "None";
    char buf[8] = {0};
    buf[0] = (char)key;
    return buf;
}

static uint16_t CharToKey(char c) {
    return static_cast<uint16_t>(static_cast<unsigned char>(c));
}

// ── New rule factory ──
Rules::Rule SettingsPanel::CreateNewRule(const std::string& name) {
    Rules::Rule rule;
    rule.Name = name;
    rule.Enabled = false;
    rule.Key = CharToKey('Q');
    rule.CooldownSec = 1.0f;
    rule.Order = 0;
    rule.ScriptBody = "// Return true when this skill should fire\n"
                      "bool CheckCondition(RadarEntity& entity) {\n"
                      "    // Example: fire on any monster with HP > 50%\n"
                      "    return entity.CurrentHP > entity.MaxHP * 0.5;\n"
                      "}";
    return rule;
}

// ── Main Draw (inline - no window, renders inside host settings) ──
void SettingsPanel::Draw(AutoReflexPlugin* plugin) {
    if (!plugin) return;

    // Restrict content width so sliders/buttons aren't clipped at right edge
    float rightMargin = 60.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, ImGui::GetStyle().ItemSpacing.y));

    // Tab bar: Rules first, Settings second (Rules selected by default)
    static const char* tabs[] = {"Rules", "Settings"};
    static int currentTab = 0;

    if (ImGui::BeginTabBar("SettingsTabs", ImGuiTabBarFlags_None)) {
        for (int i = 0; i < 2; i++) {
            if (ImGui::BeginTabItem(tabs[i])) {
                currentTab = i;
                // Offset content to leave right margin
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - rightMargin);
                switch (i) {
                    case 0: DrawRulesCombined(plugin, rightMargin); break;
                    case 1: DrawGeneralTab(plugin, rightMargin); break;
                }
                ImGui::PopItemWidth();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::PopStyleVar();
}

// ── General Tab ──
void SettingsPanel::DrawGeneralTab(AutoReflexPlugin* plugin, float rightMargin) {
    // We access plugin members via friend-like approach
    // These settings are on AutoReflexPlugin as public-accessible through DrawSettings

    ImGui::Text("AutoReflex Settings");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Checkbox("Enable Overlay", &plugin->m_OverlayEnabled);
    ImGui::SliderFloat("Window Opacity", &plugin->m_WindowAlpha, 0.3f, 1.0f, "%.2f");

    ImGui::SliderFloat("Global Cooldown", &plugin->m_GlobalCooldown, 0.1f, 3.0f, "%.2fs");
    ImGui::SliderFloat("Key Hold Duration", &plugin->m_KeyHoldDuration, 0.01f, 0.2f, "%.3fs");

    ImGui::Spacing();
    ImGui::Checkbox("Test Fire (Q)", &plugin->m_TestFireEnabled);
    ImGui::SameLine();
    ImGui::Text("Cooldown: %.1fms", plugin->m_TestFireCooldownSec * 1000.f);

    if (plugin->m_TestFireEnabled) {
        auto now = std::chrono::steady_clock::now();
        if (now - plugin->m_LastTestFire >= std::chrono::milliseconds(static_cast<uint32_t>(plugin->m_TestFireCooldownSec * 1000.0f))) {
            // Press key is handled in DrawSettings already, but keep here as fallback
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Panels:");
    ImGui::Checkbox("Show Entity List", &plugin->m_ShowEntityList);
    ImGui::Checkbox("Show Monster Detail", &plugin->m_ShowMonsterDetail);
    ImGui::Checkbox("Show Debug Log", &plugin->m_ShowDebugLog);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Save / Load buttons
    if (ImGui::Button("Save All", ImVec2(100, 0))) {
        plugin->SaveSettings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Rules", ImVec2(120, 0))) {
        plugin->LoadSettings();
    }
}

// ── Combined Rules Tab (list + inline editor below) ──
void SettingsPanel::DrawRulesCombined(AutoReflexPlugin* plugin, float rightMargin) {
    if (!plugin->m_RuleManager) {
        ImGui::Text("RuleManager not initialized. Enable the plugin first.");
        return;
    }

    auto& rules = plugin->m_RuleManager->GetRules();

    // ---- Left: Rule list ----
    ImVec2 listSize = ImGui::GetContentRegionAvail();
    listSize.y = 180.0f;

    ImGui::BeginChild("RuleList", listSize, true);
    for (size_t i = 0; i < rules.size(); i++) {
        auto& rule = rules[i];
        bool selected = (i == (size_t)plugin->m_SelectedRuleIndex);
        ImGui::PushID((int)i);

        // Checkbox to toggle enabled inline
        if (ImGui::Checkbox("", &rule.Enabled)) {
            if (plugin->m_RuleStore) plugin->m_RuleStore->SaveRule(rule);
        }
        ImGui::SameLine();

        std::string displayName = rule.Name;
        if (!rule.CompileError.empty()) {
            displayName += " !";
        }
        if (ImGui::Selectable(displayName.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
            plugin->m_SelectedRuleIndex = (int)i;
        }
        // Status indicator after name
        if (!rule.CompileError.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[Error]");
        } else if (rule.Enabled) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Ready");
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::Spacing();

    // ---- Toolbar ----
    if (ImGui::Button("New", ImVec2(70, 0))) {
        std::string newName = "New Rule";
        int counter = 1;
        for (const auto& r : rules) {
            if (r.Name.find("New Rule") == 0) counter++;
        }
        newName += std::to_string(counter);
        auto rule = CreateNewRule(newName);
        rule.Order = (int)rules.size();
        rules.push_back(std::move(rule));
        if (plugin->m_RuleStore) plugin->m_RuleStore->SaveRule(rules.back());
        plugin->m_SelectedRuleIndex = (int)rules.size() - 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete", ImVec2(80, 0)) && plugin->m_SelectedRuleIndex >= 0 && plugin->m_SelectedRuleIndex < (int)rules.size()) {
        int idx = plugin->m_SelectedRuleIndex;
        if (plugin->m_RuleStore) plugin->m_RuleStore->DeleteRule(rules[idx].Name);
        rules.erase(rules.begin() + idx);
        if (rules.empty()) {
            plugin->m_SelectedRuleIndex = -1;
        } else {
            plugin->m_SelectedRuleIndex = std::min(idx, (int)rules.size() - 1);
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Up^") && plugin->m_SelectedRuleIndex > 0) {
        std::swap(rules[plugin->m_SelectedRuleIndex], rules[plugin->m_SelectedRuleIndex - 1]);
        std::swap(rules[plugin->m_SelectedRuleIndex].Order, rules[plugin->m_SelectedRuleIndex - 1].Order);
        plugin->m_SelectedRuleIndex--;
        if (plugin->m_RuleStore) { for (auto& r : rules) plugin->m_RuleStore->SaveRule(r); }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Downv") && plugin->m_SelectedRuleIndex >= 0 && plugin->m_SelectedRuleIndex < (int)rules.size() - 1) {
        std::swap(rules[plugin->m_SelectedRuleIndex], rules[plugin->m_SelectedRuleIndex + 1]);
        std::swap(rules[plugin->m_SelectedRuleIndex].Order, rules[plugin->m_SelectedRuleIndex + 1].Order);
        plugin->m_SelectedRuleIndex++;
        if (plugin->m_RuleStore) { for (auto& r : rules) plugin->m_RuleStore->SaveRule(r); }
    }

    ImGui::Separator();
    ImGui::Spacing();

    // ---- Right side: Inline editor for selected rule ----
    if (plugin->m_SelectedRuleIndex < 0 || plugin->m_SelectedRuleIndex >= (int)rules.size()) {
        ImGui::Text("No rule selected. Select a rule above or create a new one.");
        return;
    }

    auto& rule = rules[plugin->m_SelectedRuleIndex];
    ImGui::PushID(plugin->m_SelectedRuleIndex);

    // Name + Key on same row (no Order field)
    char nameBuf[256];
    strncpy(nameBuf, rule.Name.c_str(), sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';

    // Use same slider width as cooldown row so key combo matches
    float rowW = ImGui::GetContentRegionAvail().x - rightMargin;
    float keyComboW = (rowW - 160.0f) * 0.45f;
    float keyLabelW = 30.0f;
    float nameLabelW = 40.0f;
    float spacing = 30.0f;
    float nameW = rowW - keyLabelW - keyComboW - nameLabelW - spacing;

    ImGui::Text("Name:");
    ImGui::SameLine();
    ImGui::PushItemWidth(nameW);
    ImGui::InputText("##name", nameBuf, sizeof(nameBuf));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("Key:");
    ImGui::SameLine();
    ImGui::PushItemWidth(keyComboW);

    // Key dropdown (lowercase display like gamehelper)
    static const char* keyItems[] = {
        "none", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
        "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12",
        "lbutton", "rbutton", "mbutton", "xbutton1", "xbutton2"
    };
    static const uint16_t keyValues[] = {
        0,
        'A','B','C','D','E','F','G','H','I','J','K','L','M',
        'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
        '0','1','2','3','4','5','6','7','8','9',
        VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
        VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2
    };
    static const int keyCount = sizeof(keyItems)/sizeof(keyItems[0]);

    int selectedKeyIdx = 0;
    for (int k = 0; k < keyCount; k++) {
        if (keyValues[k] == rule.Key) { selectedKeyIdx = k; break; }
    }

    if (ImGui::BeginCombo("##keyCombo", keyItems[selectedKeyIdx], ImGuiComboFlags_HeightLarge)) {
        for (int k = 0; k < keyCount; k++) {
            bool is_selected = (k == selectedKeyIdx);
            if (ImGui::Selectable(keyItems[k], is_selected)) {
                rule.Key = keyValues[k];
                selectedKeyIdx = k;
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(40.0f, 0.0f)); // Spacer after key dropdown

    // Animation Wait + Cooldown on single row (use same rowW as key row)
    float animWaitSec = rule.WaitAfterPressMs / 1000.0f;
    float sliderW = (rowW - 160.0f) * 0.45f;

    ImGui::Text("Animation Wait:");
    ImGui::SameLine();
    ImGui::PushItemWidth(sliderW);
    ImGui::SliderFloat("##animWait", &animWaitSec, 0.01f, 1.0f, "%.2f s");
    ImGui::PopItemWidth();
    rule.WaitAfterPressMs = animWaitSec * 1000.0f;
    ImGui::SameLine();
    ImGui::Text("Cooldown:");
    ImGui::SameLine();
    ImGui::PushItemWidth(sliderW);
    ImGui::SliderFloat("##cooldown", &rule.CooldownSec, 0.1f, 30.0f, "%.2f s");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(40.0f, 0.0f)); // Spacer after cooldown slider

    // Script
    ImGui::Text("Script:");
    ImGui::SameLine();
    if (ImGui::SmallButton("Compile")) plugin->m_RuleManager->CompileRule(rule);
    ImGui::SameLine();
    if (!rule.CompileError.empty()) {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", rule.CompileError.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.3f, 1, 0.3f, 1), "OK");
    }

    ImVec2 editorSize = ImGui::GetContentRegionAvail();
    editorSize.y = 150.0f;
    ImGui::BeginChild("ScriptEditor", editorSize, true);
    char scriptBuf[16384];
    strncpy(scriptBuf, rule.ScriptBody.c_str(), sizeof(scriptBuf) - 1);
    scriptBuf[sizeof(scriptBuf) - 1] = '\0';
    if (ImGui::InputTextMultiline("##script", scriptBuf, sizeof(scriptBuf), ImVec2(-1, -1))) {
        rule.ScriptBody = scriptBuf;
    }
    ImGui::EndChild();

    ImGui::Spacing();

    // Save buttons
    if (ImGui::Button("Save", ImVec2(105, 0)) && plugin->m_RuleStore) {
        plugin->m_RuleStore->SaveRule(rule);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save & Compile", ImVec2(160, 0))) {
        if (plugin->m_RuleStore) plugin->m_RuleStore->SaveRule(rule);
        plugin->m_RuleManager->CompileRule(rule);
    }

    // Status
    ImGui::Separator();
    ImGui::Text("Status: %s | Ever Fired: %s | Last Eval: %s",
        rule.Enabled ? "Enabled" : "Disabled",
        rule.EverFired ? "Yes" : "No",
        rule.LastEvalResult ? "TRUE" : "FALSE");

    ImGui::PopID();
}

} // namespace UI
} // namespace AutoReflex