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
    rule.ScriptBody = "// EXPRTK boolean expression (always true test)\n"
                      "// Available: e_IsValid, e_CurrentHP, e_MaxHP, e_IsSleeping, e_WorldX, e_WorldY, etc.\n"
                      "1";
    return rule;
}

// ── Main Draw (inline - no window, renders inside host settings) ──
void SettingsPanel::Draw(AutoReflexPlugin* plugin) {
    if (!plugin) return;

    // Restrict content width so sliders/buttons aren't clipped at right edge
    float rightMargin = 60.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, ImGui::GetStyle().ItemSpacing.y));

    // Tab bar: Rules, Settings, Debug Log
    static const char* tabs[] = {"Rules", "Settings", "Debug Log"};
    static int currentTab = 0;

    if (ImGui::BeginTabBar("SettingsTabs", ImGuiTabBarFlags_None)) {
        for (int i = 0; i < 3; i++) {
            if (ImGui::BeginTabItem(tabs[i])) {
                currentTab = i;
                // Offset content to leave right margin
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - rightMargin);
                switch (i) {
                    case 0: DrawRulesCombined(plugin, rightMargin); break;
                    case 1: DrawGeneralTab(plugin, rightMargin); break;
                    case 2: DrawDebugLogTab(plugin, rightMargin); break;
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

// ── Debug Log tab (same ring buffer as overlay `Log()`) ──
void SettingsPanel::DrawDebugLogTab(AutoReflexPlugin* plugin, float /*rightMargin*/) {
    ImGui::TextUnformatted("Runtime messages from Log() (rules, keys, load, etc.).");
    ImGui::Spacing();

    if (ImGui::Checkbox("Enable##DebugLogTab", &plugin->m_DebugLogTabEnabled)) {
        // toggled
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(turn on to show the log below)");

    if (!plugin->m_DebugLogTabEnabled)
        return;

    ImGui::Separator();
    ImGui::Text("Entries: %zu (max %zu)", plugin->m_DebugLog.size(), (size_t)AutoReflexPlugin::DEBUG_LOG_MAX);
    if (ImGui::SmallButton("Clear##DebugLogTab")) {
        plugin->m_DebugLog.clear();
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.y = std::max(120.0f, avail.y - ImGui::GetStyle().ItemSpacing.y);
    ImGui::BeginChild("DebugLogTabScroll", avail, true, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& entry : plugin->m_DebugLog) {
        ImGui::TextUnformatted(entry.c_str());
    }
    if (plugin->m_DebugLog.empty()) {
        ImGui::TextDisabled("No messages yet. Enable the plugin and use rules / overlay to generate log lines.");
    }
    ImGui::EndChild();
}

// ── Combined Rules Tab: rule list split 50/50 (list | compile errors), detail below full width ──
void SettingsPanel::DrawRulesCombined(AutoReflexPlugin* plugin, float rightMargin) {
    if (!plugin->m_RuleManager) {
        ImGui::Text("RuleManager not initialized. Enable the plugin first.");
        return;
    }

    auto& rules = plugin->m_RuleManager->GetRules();

    float totalW = ImGui::GetContentRegionAvail().x - rightMargin;
    float listLeftW = totalW * 0.5f;   // 50% rule list
    float listRightW = totalW * 0.5f;  // 50% compile output
    float listH = 160.0f;              // Fixed height for the split row

    // ═══════════════════════════════════════════════════════
    // TOP ROW — Rule list (50%) | Compile output (50%)
    // ═══════════════════════════════════════════════════════
    ImGui::Text("Rules");
    ImGui::Separator();

    // Left: Rule list
    ImGui::BeginChild("RuleList", ImVec2(listLeftW, listH), true);
    for (size_t i = 0; i < rules.size(); i++) {
        auto& rule = rules[i];
        bool selected = (i == (size_t)plugin->m_SelectedRuleIndex);
        ImGui::PushID((int)i);

        if (ImGui::Checkbox("", &rule.Enabled)) {
            if (plugin->m_RuleStore) plugin->m_RuleStore->SaveRule(rule);
        }
        ImGui::SameLine();

        std::string displayName = rule.Name;
        if (!rule.CompileError.empty()) displayName += " !";
        if (ImGui::Selectable(displayName.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
            plugin->m_SelectedRuleIndex = (int)i;
        }
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

    // Right: Compile output panel (copyable via InputTextMultiline)
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(4.0f, 0.0f)); // tiny gap
    ImGui::SameLine();
    ImGui::BeginChild("CompileErrorPanel", ImVec2(listRightW, listH), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::Text("Compile Output");
    ImGui::Separator();
    ImGui::Spacing();

    if (plugin->m_SelectedRuleIndex >= 0 && plugin->m_SelectedRuleIndex < (int)rules.size()) {
        const auto& curRule = rules[plugin->m_SelectedRuleIndex];
        ImVec4 textColor;
        char compileBuf[8192] = {0};
        if (curRule.CompileError.empty()) {
            strcpy(compileBuf, "Compilation OK — no errors.");
            textColor = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        } else {
            strncpy(compileBuf, curRule.CompileError.c_str(), sizeof(compileBuf) - 1);
            textColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
        ImGui::InputTextMultiline("##CompileOutput", compileBuf, sizeof(compileBuf), ImVec2(-1, -1));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No rule selected.");
    }
    ImGui::EndChild();

    ImGui::Spacing();

    // ═══════════════════════════════════════════════════════
    // TOOLBAR — full width
    // ═══════════════════════════════════════════════════════
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

    ImGui::SameLine();
    if (plugin->m_SelectedRuleIndex >= 0 && plugin->m_SelectedRuleIndex < (int)rules.size()) {
        const auto& sr = rules[plugin->m_SelectedRuleIndex];
        ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "%s | %s | %s",
            sr.Enabled?"Enabled":"Disabled", sr.EverFired?"Fired":"Not Fired",
            sr.LastEvalResult?"Eval=TRUE":"Eval=FALSE");
    }

    ImGui::Separator();
    ImGui::Spacing();

    // ═══════════════════════════════════════════════════════
    // RULE DETAIL — full width below
    // ═══════════════════════════════════════════════════════
    if (plugin->m_SelectedRuleIndex < 0 || plugin->m_SelectedRuleIndex >= (int)rules.size()) {
        ImGui::Text("No rule selected. Select a rule above or create a new one.");
        return;
    }

    auto& rule = rules[plugin->m_SelectedRuleIndex];
    ImGui::PushID(plugin->m_SelectedRuleIndex);

    float rowW = ImGui::GetContentRegionAvail().x - rightMargin;

    // Name + Key row
    char nameBuf[256];
    strncpy(nameBuf, rule.Name.c_str(), sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    float keyComboW = rowW * 0.4f;
    float nameW = rowW - 40.0f - 30.0f - keyComboW - 20.0f;

    ImGui::Text("Name:");
    ImGui::SameLine();
    ImGui::PushItemWidth(nameW);
    ImGui::InputText("##name", nameBuf, sizeof(nameBuf));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("Key:");
    ImGui::SameLine();
    ImGui::PushItemWidth(keyComboW);

    static const char* keyItems[] = {
        "none","a","b","c","d","e","f","g","h","i","j","k","l","m",
        "n","o","p","q","r","s","t","u","v","w","x","y","z",
        "0","1","2","3","4","5","6","7","8","9",
        "f1","f2","f3","f4","f5","f6","f7","f8","f9","f10","f11","f12",
        "lbutton","rbutton","mbutton","xbutton1","xbutton2"
    };
    static const uint16_t keyValues[] = {
        0,'A','B','C','D','E','F','G','H','I','J','K','L','M',
        'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
        '0','1','2','3','4','5','6','7','8','9',
        VK_F1,VK_F2,VK_F3,VK_F4,VK_F5,VK_F6,VK_F7,VK_F8,VK_F9,VK_F10,VK_F11,VK_F12,
        VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2
    };
    static const int keyCount = sizeof(keyItems)/sizeof(keyItems[0]);
    int selectedKeyIdx = 0;
    for (int k = 0; k < keyCount; k++) { if (keyValues[k] == rule.Key) { selectedKeyIdx = k; break; } }

    if (ImGui::BeginCombo("##keyCombo", keyItems[selectedKeyIdx], ImGuiComboFlags_HeightLarge)) {
        for (int k = 0; k < keyCount; k++) {
            bool is_sel = (k == selectedKeyIdx);
            if (ImGui::Selectable(keyItems[k], is_sel)) { rule.Key = keyValues[k]; selectedKeyIdx = k; }
            if (is_sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    // Anim Wait + Cooldown
    float animWaitSec = rule.WaitAfterPressMs / 1000.0f;
    float sliderW = rowW * 0.4f;
    ImGui::Text("Anim Wait:");
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

    // Save / Compile
    ImGui::Spacing();
    if (ImGui::Button("Save", ImVec2(105, 0)) && plugin->m_RuleStore) plugin->m_RuleStore->SaveRule(rule);
    ImGui::SameLine();
    if (ImGui::Button("Save & Compile", ImVec2(160, 0))) {
        if (plugin->m_RuleStore) plugin->m_RuleStore->SaveRule(rule);
        plugin->m_RuleManager->CompileRule(rule);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Compile")) plugin->m_RuleManager->CompileRule(rule);
    if (!rule.CompileError.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "Error");
    } else if (rule.CompiledExpr && rule.CompiledExpr->IsValid()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f,1,0.3f,1), "OK");
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Script editor fills remaining height
    ImGui::Text("Script:");
    ImVec2 editorSize = ImGui::GetContentRegionAvail();
    editorSize.y -= 8.0f;
    if (editorSize.y < 80.0f) editorSize.y = 80.0f;
    ImGui::BeginChild("ScriptEditor", editorSize, true);
    char scriptBuf[16384];
    strncpy(scriptBuf, rule.ScriptBody.c_str(), sizeof(scriptBuf) - 1);
    scriptBuf[sizeof(scriptBuf) - 1] = '\0';
    if (ImGui::InputTextMultiline("##script", scriptBuf, sizeof(scriptBuf), ImVec2(-1, -1))) {
        rule.ScriptBody = scriptBuf;
    }
    ImGui::EndChild();

    ImGui::PopID();
}

} // namespace UI
} // namespace AutoReflex