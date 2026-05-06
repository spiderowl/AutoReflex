// AutoReflex - SettingsPanel.cpp
// Settings panel with tabs: Rules, Settings.
//
// Text inputs bind directly to std::string via ImGuiInputTextFlags_CallbackResize
// so we don't pay for a 16 KB stack copy + std::string assign on every frame
// the editor is open.

#include "SettingsPanel.h"
#include "../AutoReflex.h"
#include "../rules/RuleManager.h"
#include "../storage/RuleStore.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

namespace AutoReflex {
namespace UI {

namespace {

// Wrappers around ImGui's official std::string adapter (imgui_stdlib).
// Both bind directly to the std::string and use the resize callback under
// the hood — no per-frame stack buffers, no extra copies.
bool InputStringMultiline(const char* label, std::string& s, const ImVec2& size) {
    return ImGui::InputTextMultiline(label, &s, size);
}
bool InputString(const char* label, std::string& s, ImGuiInputTextFlags flags = 0) {
    return ImGui::InputText(label, &s, flags);
}

} // namespace

Rules::Rule SettingsPanel::CreateNewRule(const std::string& name) {
    Rules::Rule rule;
    rule.Name        = name;
    rule.Enabled     = false;
    rule.Key         = static_cast<uint16_t>('Q');
    rule.CooldownSec = 1.0f;
    rule.Order       = 0;
    rule.ScriptBody  = "monsterCount > 0";
    return rule;
}

void SettingsPanel::Draw(AutoReflexPlugin* plugin) {
    if (!plugin) return;

    constexpr float kRightMargin = 60.0f;
    static const char* const kTabs[] = { "Rules", "Settings" };

    if (ImGui::BeginTabBar("SettingsTabs", ImGuiTabBarFlags_None)) {
        for (int i = 0; i < IM_ARRAYSIZE(kTabs); ++i) {
            if (ImGui::BeginTabItem(kTabs[i])) {
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - kRightMargin);
                switch (i) {
                    case 0: DrawRulesCombined(plugin, kRightMargin); break;
                    case 1: DrawGeneralTab   (plugin, kRightMargin); break;
                }
                ImGui::PopItemWidth();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

void SettingsPanel::DrawGeneralTab(AutoReflexPlugin* plugin, float rightMargin) {
    ImGui::Text("AutoReflex Settings");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::BeginTable("GeneralSliders", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX)) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

        auto sliderInt = [&](const char* label, const char* id, int* v, int v_min, int v_max, const char* fmt) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            float w = ImGui::GetContentRegionAvail().x - rightMargin;
            if (w < 40.0f) w = 40.0f;
            ImGui::SetNextItemWidth(w);
            ImGui::SliderInt(id, v, v_min, v_max, fmt);
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::Dummy(ImVec2(rightMargin, 0.0f));
        };

        // Eval interval: how often EvaluateAll runs, regardless of FPS.
        // Lower = lower latency; higher = less CPU. 16ms = ~60 Hz.
        sliderInt("Eval Interval (ms)", "##EvalIntervalMs",  &plugin->m_EvalIntervalMs,  0, 100, "%d ms");

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Checkbox("Test Fire (Q)", &plugin->m_TestFireEnabled);
    ImGui::SameLine();
    ImGui::Text("Cooldown: %.1fms", plugin->m_TestFireCooldownSec * 1000.f);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Save All", ImVec2(100, 0))) {
        plugin->SaveSettings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Rules", ImVec2(120, 0))) {
        plugin->LoadSettings();
    }
}

void SettingsPanel::DrawRulesCombined(AutoReflexPlugin* plugin, float rightMargin) {
    if (!plugin->m_RuleManager) {
        ImGui::Text("RuleManager not initialized. Enable the plugin first.");
        return;
    }

    auto& rules = plugin->m_RuleManager->GetRules();

    const float totalW = ImGui::GetContentRegionAvail().x - rightMargin;
    const float listLeftW  = totalW * 0.5f;
    const float listRightW = totalW * 0.5f;
    const float listH      = 320.0f;

    ImGui::BeginChild("RuleList", ImVec2(listLeftW, listH), true);
    for (size_t i = 0; i < rules.size(); ++i) {
        auto& rule = rules[i];
        const bool selected = (static_cast<int>(i) == plugin->m_SelectedRuleIndex);
        ImGui::PushID(static_cast<int>(i));

        if (ImGui::Checkbox("", &rule.Enabled)) {
            if (plugin->m_RuleStore) plugin->m_RuleStore->SaveRule(rule);
        }
        ImGui::SameLine();

        std::string displayName = rule.Name;
        if (!rule.CompileError.empty()) displayName += " !";
        if (ImGui::Selectable(displayName.c_str(), selected,
                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
            plugin->m_SelectedRuleIndex = static_cast<int>(i);
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

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(4.0f, 0.0f));
    ImGui::SameLine();
    ImGui::BeginChild("CompileOutputPanel", ImVec2(listRightW, listH), true,
        ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::Text("Compile Output");
    ImGui::Separator();
    ImGui::Spacing();

    if (plugin->m_SelectedRuleIndex >= 0 && plugin->m_SelectedRuleIndex < static_cast<int>(rules.size())) {
        const auto& curRule = rules[plugin->m_SelectedRuleIndex];
        ImVec4 textColor;
        std::string compileText;
        if (curRule.CompileError.empty()) {
            compileText = "Compilation OK — no errors.";
            textColor = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        } else {
            compileText = curRule.CompileError;
            textColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        }
        const float footerH = ImGui::GetFrameHeightWithSpacing();
        ImVec2 editSize = ImGui::GetContentRegionAvail();
        editSize.y = std::max(48.0f, editSize.y - footerH);

        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
        ImGui::InputTextMultiline("##CompileOutput", &compileText, editSize,
            ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        if (ImGui::SmallButton("Copy")) ImGui::SetClipboardText(compileText.c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1), "read-only");
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No rule selected.");
    }
    ImGui::EndChild();

    ImGui::Spacing();

    // Toolbar
    const ImVec2 toolbarBtnSize(80.0f, 0.0f);
    if (ImGui::Button("New", toolbarBtnSize)) {
        std::string newName = "New Rule";
        int counter = 1;
        for (const auto& r : rules) {
            if (r.Name.find("New Rule") == 0) ++counter;
        }
        newName += std::to_string(counter);
        auto rule = CreateNewRule(newName);
        rule.Order = static_cast<int>(rules.size());
        rules.push_back(std::move(rule));
        if (plugin->m_RuleStore) plugin->m_RuleStore->SaveRule(rules.back());
        plugin->m_SelectedRuleIndex = static_cast<int>(rules.size()) - 1;
    }
    ImGui::SameLine();
    static bool confirmDeleteOpen = false;
    static std::string pendingDeleteName;
    if (ImGui::Button("Delete", toolbarBtnSize)
        && plugin->m_SelectedRuleIndex >= 0
        && plugin->m_SelectedRuleIndex < static_cast<int>(rules.size())) {
        pendingDeleteName = rules[plugin->m_SelectedRuleIndex].Name;
        confirmDeleteOpen = true;
        ImGui::OpenPopup("Delete Rule?");
    }
    ImGui::SameLine();
    if (ImGui::Button("Up", toolbarBtnSize) && plugin->m_SelectedRuleIndex > 0) {
        const int i = plugin->m_SelectedRuleIndex;
        std::swap(rules[i], rules[i - 1]);
        std::swap(rules[i].Order, rules[i - 1].Order);
        plugin->m_SelectedRuleIndex--;
        if (plugin->m_RuleStore) {
            plugin->m_RuleStore->SaveRule(rules[i]);
            plugin->m_RuleStore->SaveRule(rules[i - 1]);
        }
        plugin->m_RuleManager->SortByOrder();
    }
    ImGui::SameLine();
    if (ImGui::Button("Down", toolbarBtnSize)
        && plugin->m_SelectedRuleIndex >= 0
        && plugin->m_SelectedRuleIndex < static_cast<int>(rules.size()) - 1) {
        const int i = plugin->m_SelectedRuleIndex;
        std::swap(rules[i], rules[i + 1]);
        std::swap(rules[i].Order, rules[i + 1].Order);
        plugin->m_SelectedRuleIndex++;
        if (plugin->m_RuleStore) {
            plugin->m_RuleStore->SaveRule(rules[i]);
            plugin->m_RuleStore->SaveRule(rules[i + 1]);
        }
        plugin->m_RuleManager->SortByOrder();
    }

    ImGui::SameLine();
    if (plugin->m_SelectedRuleIndex >= 0 && plugin->m_SelectedRuleIndex < static_cast<int>(rules.size())) {
        const auto& sr = rules[plugin->m_SelectedRuleIndex];
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1), "%s | %s | %s",
            sr.Enabled ? "Enabled" : "Disabled",
            sr.EverFired ? "Fired" : "Not Fired",
            sr.LastEvalResult ? "Eval=TRUE" : "Eval=FALSE");
    }

    ImGui::Separator();
    ImGui::Spacing();

    if (confirmDeleteOpen) {
        if (ImGui::BeginPopupModal("Delete Rule?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete rule \"%s\"?", pendingDeleteName.c_str());
            ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1), "This cannot be undone.");
            ImGui::Spacing();
            if (ImGui::Button("Cancel")) {
                confirmDeleteOpen = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                int idx = plugin->m_SelectedRuleIndex;
                if (idx >= 0 && idx < static_cast<int>(rules.size()) && rules[idx].Name == pendingDeleteName) {
                    if (plugin->m_RuleStore) plugin->m_RuleStore->DeleteRule(rules[idx].Name);
                    rules.erase(rules.begin() + idx);
                    plugin->m_SelectedRuleIndex = rules.empty()
                        ? -1
                        : std::min(idx, static_cast<int>(rules.size()) - 1);
                }
                confirmDeleteOpen = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    if (plugin->m_SelectedRuleIndex < 0 || plugin->m_SelectedRuleIndex >= static_cast<int>(rules.size())) {
        ImGui::Text("No rule selected. Select a rule above or create a new one.");
        return;
    }

    auto& rule = rules[plugin->m_SelectedRuleIndex];
    ImGui::PushID(plugin->m_SelectedRuleIndex);

    const float rowW = ImGui::GetContentRegionAvail().x - rightMargin;

    auto isValidRuleName = [](const std::string& s) -> bool {
        if (s.empty()) return false;
        const std::string bad = "<>:\"/\\\\|?*";
        if (s.find_first_of(bad) != std::string::npos) return false;
        if (s.back() == '.' || s.back() == ' ') return false;
        return true;
    };

    static std::string lastRenameError;

    // Name + Key row.
    // The name buffer is held locally and only synced into rule.Name on commit
    // (IsItemDeactivatedAfterEdit) — so we don't reallocate on every keystroke.
    static int  s_LastEditedIdx = -1;
    static std::string s_NameDraft;
    if (s_LastEditedIdx != plugin->m_SelectedRuleIndex || !ImGui::IsAnyItemActive()) {
        s_NameDraft = rule.Name;
        s_LastEditedIdx = plugin->m_SelectedRuleIndex;
    }

    const float keyComboW = rowW * 0.4f;
    const float nameW     = rowW - 40.0f - 30.0f - keyComboW - 20.0f;

    ImGui::Text("Name:");
    ImGui::SameLine();
    ImGui::PushItemWidth(nameW);
    InputString("##name", s_NameDraft);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        auto trimAscii = [](std::string s) {
            size_t a = 0;
            while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
            size_t b = s.size();
            while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
            return s.substr(a, b - a);
        };

        std::string newName = trimAscii(s_NameDraft);
        const std::string oldName = rule.Name;
        if (!newName.empty() && newName != oldName) {
            if (!isValidRuleName(newName)) {
                lastRenameError = "Invalid rule name (file name unsafe). Avoid <>:\"/\\|?* and trailing dot/space.";
            } else {
                bool renamed = false;
                if (plugin->m_RuleStore) renamed = plugin->m_RuleStore->RenameRule(oldName, newName);
                if (renamed) {
                    lastRenameError.clear();
                    rule.Name = newName;
                    if (plugin->m_RuleStore) plugin->m_RuleStore->SaveRule(rule);
                } else {
                    lastRenameError = "Rename failed (name already exists or file is locked).";
                }
            }
        }
    }
    ImGui::PopItemWidth();
    if (!lastRenameError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s", lastRenameError.c_str());
    }

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
    static const int keyCount = sizeof(keyItems) / sizeof(keyItems[0]);
    int selectedKeyIdx = 0;
    for (int k = 0; k < keyCount; ++k) {
        if (keyValues[k] == rule.Key) { selectedKeyIdx = k; break; }
    }

    if (ImGui::BeginCombo("##keyCombo", keyItems[selectedKeyIdx], ImGuiComboFlags_HeightLarge)) {
        for (int k = 0; k < keyCount; ++k) {
            const bool is_sel = (k == selectedKeyIdx);
            if (ImGui::Selectable(keyItems[k], is_sel)) { rule.Key = keyValues[k]; selectedKeyIdx = k; }
            if (is_sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    // Anim Wait + Cooldown
    float animWaitSec = rule.WaitAfterPressMs / 1000.0f;
    const float sliderW = rowW * 0.4f;
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

    ImGui::Spacing();
    if (ImGui::Button("Compile & Save", ImVec2(220, 0))) {
        if (plugin->m_RuleStore) plugin->m_RuleStore->SaveRule(rule);
        plugin->m_RuleManager->CompileRule(rule);
    }
    if (!rule.CompileError.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error");
    } else if (rule.CompiledExpr && rule.CompiledExpr->IsValid()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 1, 0.3f, 1), "OK");
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Script editor — InputTextMultiline binds to rule.ScriptBody directly via
    // ImGui's std::string adapter. No 16 KB stack copy on every frame.
    ImGui::Text("Script:");
    ImVec2 editorSize = ImGui::GetContentRegionAvail();
    editorSize.y -= 8.0f;
    if (editorSize.y < 80.0f) editorSize.y = 80.0f;
    ImGui::BeginChild("ScriptEditor", editorSize, true);
    InputStringMultiline("##script", rule.ScriptBody, ImVec2(-1, -1));
    ImGui::EndChild();

    ImGui::PopID();
}

} // namespace UI
} // namespace AutoReflex
