// EntityList.h - Monster/entity selection list UI
// Displays entities from GetEntityDebugList in a scrollable selectable list

#pragma once

#include "../sdk/PluginContext.h"
#include <vector>

namespace PluginSDK {
    struct DebugEntityInfo;
}

namespace AutoReflex {

// Collect monsters/NPCs from debug entity list
// Returns indices into the debugEntities vector
std::vector<size_t> CollectMonsters(const std::vector<PluginSDK::DebugEntityInfo>& debugEntities);

// Render the entity selection list panel
// Returns the index of the selected entity in the monsterIndices array, or -1
int DrawEntityList(const std::vector<PluginSDK::DebugEntityInfo>& debugEntities, int currentSelection);

} // namespace AutoReflex