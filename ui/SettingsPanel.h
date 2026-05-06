// AutoReflex - SettingsPanel UI
// Settings panel rendered in DrawSettings(). Tabs: Rules, Settings.

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
