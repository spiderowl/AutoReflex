// AutoReflex - SettingsPanel UI
// Full settings panel with tabs: General, Rules, Script Docs

#pragma once

#include "../rules/Rule.h"

class AutoReflexPlugin;

namespace AutoReflex {
namespace UI {

class SettingsPanel {
public:
    static void Draw(AutoReflexPlugin* plugin);

private:
    static void DrawGeneralTab(AutoReflexPlugin* plugin, float rightMargin);
    static void DrawRulesCombined(AutoReflexPlugin* plugin, float rightMargin);

    // Create a new rule with defaults
    static AutoReflex::Rules::Rule CreateNewRule(const std::string& name);
};

} // namespace UI
} // namespace AutoReflex