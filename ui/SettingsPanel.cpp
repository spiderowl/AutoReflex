// AutoReflex - SettingsPanel.cpp
// Settings panel with tabs: Rules, Settings.
//
// Text inputs bind directly to std::string via ImGuiInputTextFlags_CallbackResize
// so we don't pay for a large stack copy + std::string assign on every frame
// the editor is open.

#include "SettingsPanel.h"
#include "../AutoReflex.h"
#include "../rules/RuleManager.h"
#include "../storage/RuleStore.h"
#include "KeyBindings.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace AutoReflex {
namespace UI {

namespace {

struct InputTextCallback_UserData
{
    std::string*            Str = nullptr;
    ImGuiInputTextCallback  ChainCallback = nullptr;
    void*                   ChainCallbackUserData = nullptr;
};

static int InputTextCallback(ImGuiInputTextCallbackData* data)
{
    auto* user_data = static_cast<InputTextCallback_UserData*>(data->UserData);
    if (!user_data || !user_data->Str) return 0;

    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        std::string* str = user_data->Str;
        str->resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = const_cast<char*>(str->c_str());
        return 0;
    }

    if (user_data->ChainCallback) {
        data->UserData = user_data->ChainCallbackUserData;
        return user_data->ChainCallback(data);
    }
    return 0;
}

// Minimal std::string adapter (same mechanism as ImGui's misc/cpp/imgui_stdlib,
// but kept local so our vendored imgui/ folder matches upstream ExamplePlugin).
static bool InputString(const char* label, std::string& str, ImGuiInputTextFlags flags = 0,
                        ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = &str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;

    return ImGui::InputText(label, const_cast<char*>(str.c_str()), str.capacity() + 1, flags, InputTextCallback, &cb_user_data);
}

static bool InputStringMultiline(const char* label, std::string& str, const ImVec2& size,
                                 ImGuiInputTextFlags flags = 0,
                                 ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = &str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;

    return ImGui::InputTextMultiline(label, const_cast<char*>(str.c_str()), str.capacity() + 1, size, flags, InputTextCallback, &cb_user_data);
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
                    case 1: DrawGeneralTab(plugin); break;
                }
                ImGui::PopItemWidth();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

void SettingsPanel::DrawGeneralTab(AutoReflexPlugin* plugin) {
    if (!plugin->HostCompatible()) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
            "Host SDK incompatible — update POEFixer or rebuild AutoReflex.");
        ImGui::Spacing();
    }

    ImGui::Text("AutoReflex Settings");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Checkbox("Show execution gate (dev)", &plugin->m_ShowGateReason);
    if (plugin->m_ShowGateReason) {
        ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "Gate: %s",
            plugin->GetLastExecutionGateReason().c_str());
    }

    ImGui::Spacing();
    {
        const char* testFireKeyName = "Q";
        if (plugin->m_SelectedRuleIndex >= 0 && plugin->m_RuleManager) {
            const auto& rules = plugin->m_RuleManager->GetRules();
            if (plugin->m_SelectedRuleIndex < static_cast<int>(rules.size())) {
                const uint16_t selectedKey = rules[static_cast<size_t>(plugin->m_SelectedRuleIndex)].Key;
                if (selectedKey > 0) {
                    testFireKeyName = KeyBindings::LabelForVirtualKey(selectedKey);
                }
            }
        }
        char testFireCheckboxLabel[64];
        std::snprintf(testFireCheckboxLabel, sizeof(testFireCheckboxLabel), "Test Fire (%s)", testFireKeyName);
        ImGui::Checkbox(testFireCheckboxLabel, &plugin->m_TestFireEnabled);
    }
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

        const float keyComboWidth = 92.0f;
        const float keyComboX = ImGui::GetWindowContentRegionMax().x - keyComboWidth;
        const float nameWidth = std::max(80.0f, keyComboX - ImGui::GetCursorPosX() - 6.0f);

        ImGui::SetNextItemWidth(nameWidth);
        std::string displayName = rule.Name;
        if (!rule.CompileError.empty()) displayName += " !";
        if (ImGui::Selectable(displayName.c_str(), selected,
                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
            plugin->m_SelectedRuleIndex = static_cast<int>(i);
        }

        ImGui::SameLine(keyComboX);
        ImGui::PushItemWidth(keyComboWidth);
        int selectedKeyIdx = 0;
        for (int keyIndex = 0; keyIndex < KeyBindings::kCount; ++keyIndex) {
            if (KeyBindings::kValues[keyIndex] == rule.Key) { selectedKeyIdx = keyIndex; break; }
        }
        if (ImGui::BeginCombo("##keyComboList", KeyBindings::kLabels[selectedKeyIdx], ImGuiComboFlags_HeightLarge)) {
            for (int keyIndex = 0; keyIndex < KeyBindings::kCount; ++keyIndex) {
                const bool isSelectedKey = (keyIndex == selectedKeyIdx);
                if (ImGui::Selectable(KeyBindings::kLabels[keyIndex], isSelectedKey)) {
                    rule.Key = KeyBindings::kValues[keyIndex];
                    selectedKeyIdx = keyIndex;
                }
                if (isSelectedKey) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

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
    ImGui::BeginChild("CompileOutputPanel", ImVec2(listRightW, listH), true);

    static std::string lastRenameError;

    auto isValidRuleName = [](const std::string& s) -> bool {
        if (s.empty()) return false;
        const std::string bad = "<>:\"/\\\\|?*";
        if (s.find_first_of(bad) != std::string::npos) return false;
        if (s.back() == '.' || s.back() == ' ') return false;
        return true;
    };

    Rules::Rule* selectedRule = nullptr;
    if (plugin->m_SelectedRuleIndex >= 0 && plugin->m_SelectedRuleIndex < static_cast<int>(rules.size())) {
        selectedRule = &rules[plugin->m_SelectedRuleIndex];
    }

    const float headerBoxHeight = 90.0f;
    ImGui::BeginChild("SelectedRuleHeader", ImVec2(0.0f, headerBoxHeight), true);
    if (!selectedRule) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No rule selected.");
    } else {
        static int s_LastEditedIdx = -1;
        static std::string s_NameDraft;
        if (s_LastEditedIdx != plugin->m_SelectedRuleIndex || !ImGui::IsAnyItemActive()) {
            s_NameDraft = selectedRule->Name;
            s_LastEditedIdx = plugin->m_SelectedRuleIndex;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Name");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        InputString("##selectedRuleName", s_NameDraft);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            auto trimAscii = [](std::string s) {
                size_t a = 0;
                while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
                size_t b = s.size();
                while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
                return s.substr(a, b - a);
            };

            std::string newName = trimAscii(s_NameDraft);
            const std::string oldName = selectedRule->Name;
            if (!newName.empty() && newName != oldName) {
                if (!isValidRuleName(newName)) {
                    lastRenameError = "Invalid rule name (file name unsafe). Avoid <>:\"/\\|?* and trailing dot/space.";
                } else {
                    bool renamed = false;
                    if (plugin->m_RuleStore) {
                        renamed = plugin->m_RuleStore->RenameRuleOnDisk(oldName, newName);
                    }
                    if (renamed) {
                        lastRenameError.clear();
                        selectedRule->Name = newName;
                        if (plugin->m_RuleStore) plugin->m_RuleStore->SaveRule(*selectedRule);
                    } else {
                        lastRenameError = "Rename failed (name already exists or file is locked).";
                    }
                }
            }
        }

        static const float cooldownOptionsSec[] = { 0.5f, 1.0f, 1.5f, 2.5f, 3.0f, 5.0f, 7.0f, 10.0f };
        static const char* cooldownOptionLabels[] = { "0.5s","1s","1.5s","2.5s","3s","5s","7s","10s" };
        static const int cooldownOptionCount = sizeof(cooldownOptionsSec) / sizeof(cooldownOptionsSec[0]);

        static const float animWaitOptionsSec[] = { 0.1f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 4.5f, 5.0f };
        static const char* animWaitOptionLabels[] = { "0.1s","0.5s","1s","1.5s","2s","2.5s","3s","3.5s","4s","4.5s","5s" };
        static const int animWaitOptionCount = sizeof(animWaitOptionsSec) / sizeof(animWaitOptionsSec[0]);

        int selectedCooldownIndex = 0;
        for (int optionIndex = 0; optionIndex < cooldownOptionCount; ++optionIndex) {
            if (std::abs(selectedRule->CooldownSec - cooldownOptionsSec[optionIndex]) < 0.001f) {
                selectedCooldownIndex = optionIndex;
                break;
            }
        }

        int selectedAnimWaitIndex = 0;
        const float animWaitSec = selectedRule->WaitAfterPressMs / 1000.0f;
        for (int optionIndex = 0; optionIndex < animWaitOptionCount; ++optionIndex) {
            if (std::abs(animWaitSec - animWaitOptionsSec[optionIndex]) < 0.001f) {
                selectedAnimWaitIndex = optionIndex;
                break;
            }
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Anim wait");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::BeginCombo("##animWaitCombo", animWaitOptionLabels[selectedAnimWaitIndex], ImGuiComboFlags_None)) {
            for (int optionIndex = 0; optionIndex < animWaitOptionCount; ++optionIndex) {
                const bool isSelected = (optionIndex == selectedAnimWaitIndex);
                if (ImGui::Selectable(animWaitOptionLabels[optionIndex], isSelected)) {
                    selectedRule->WaitAfterPressMs = animWaitOptionsSec[optionIndex] * 1000.0f;
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::TextUnformatted("Cooldown");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::BeginCombo("##cooldownCombo", cooldownOptionLabels[selectedCooldownIndex], ImGuiComboFlags_None)) {
            for (int optionIndex = 0; optionIndex < cooldownOptionCount; ++optionIndex) {
                const bool isSelected = (optionIndex == selectedCooldownIndex);
                if (ImGui::Selectable(cooldownOptionLabels[optionIndex], isSelected)) {
                    selectedRule->CooldownSec = cooldownOptionsSec[optionIndex];
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (!lastRenameError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s", lastRenameError.c_str());
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();

    ImGui::BeginChild("CompileOutputBody", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (selectedRule) {
        ImVec4 textColor;
        std::string compileText;
        if (selectedRule->CompileError.empty()) {
            compileText = "Compilation OK — no errors.";
            textColor = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        } else {
            compileText = selectedRule->CompileError;
            textColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        }

        const float footerH = ImGui::GetFrameHeightWithSpacing();
        ImVec2 editSize = ImGui::GetContentRegionAvail();
        editSize.y = std::max(48.0f, editSize.y - footerH);

        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
        InputStringMultiline("##CompileOutput", compileText, editSize, ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        if (ImGui::SmallButton("Copy")) ImGui::SetClipboardText(compileText.c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1), "read-only");
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No rule selected.");
    }
    ImGui::EndChild();

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
        plugin->m_RuleManager->SortRulesByEvaluationOrder();
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
        plugin->m_RuleManager->SortRulesByEvaluationOrder();
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
                    if (plugin->m_RuleStore) {
                        plugin->m_RuleStore->DeleteRuleFromDiskByName(rules[idx].Name);
                    }
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

    // Script editor — InputTextMultiline binds to rule.ScriptBody directly via
    // ImGui's std::string adapter. No 16 KB stack copy on every frame.
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Script:");
    ImGui::SameLine();
    if (ImGui::Button("Compile & Save", ImVec2(160, 0))) {
        if (plugin->m_RuleStore) plugin->m_RuleStore->SaveRule(rule);
        plugin->m_RuleManager->CompileRuleExpression(rule);
    }
    if (!rule.CompileError.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error");
    } else if (rule.CompiledExpr && rule.CompiledExpr->HasCompiledExpression()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 1, 0.3f, 1), "OK");
    }

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
