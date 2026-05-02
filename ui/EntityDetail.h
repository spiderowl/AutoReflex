// EntityDetail.h - Selected entity detail panel with components tree
// Renders entity info and expandable components (Life, Buffs, Render, etc.)

#pragma once

#include "../sdk/PluginContext.h"
#include "../sdk/PluginGameData.h"
#include <vector>

namespace PluginSDK {
    struct DebugEntityInfo;
}

namespace AutoReflex {

// Render the entity detail panel for a selected entity
// Manages WatchEntity/UnwatchEntity lifecycle (watch on open, unwatch on close)
void DrawEntityDetail(
    PluginContext* context,
    const PluginSDK::DebugEntityInfo& entity,
    uint32_t& watchedEntityId,
    int& watchFrameCounter);

} // namespace AutoReflex