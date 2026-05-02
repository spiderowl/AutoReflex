// EntityList.cpp - Monster/entity selection list UI implementation

#include "EntityList.h"
#include "../sdk/PluginGameData.h"
#include "../imgui/imgui.h"

namespace AutoReflex {

std::vector<size_t> CollectMonsters(const std::vector<PluginSDK::DebugEntityInfo>& debugEntities) {
    std::vector<size_t> indices;
    for (size_t i = 0; i < debugEntities.size(); i++) {
        // EntityType 2 = NPC, 5 = Monster
        if (debugEntities[i].EntityType == 2 || debugEntities[i].EntityType == 5) {
            indices.push_back(i);
        }
    }
    return indices;
}

int DrawEntityList(const std::vector<PluginSDK::DebugEntityInfo>& debugEntities, int currentSelection) {
    auto monsterIndices = CollectMonsters(debugEntities);

    ImGui::Text("Monsters: %d (of %zu total)", (int)monsterIndices.size(), debugEntities.size());

    ImGui::BeginChild("MonsterList", ImVec2(0, 200), true);
    int selectedIdx = currentSelection;

    for (size_t idx = 0; idx < monsterIndices.size(); idx++) {
        const auto& entity = debugEntities[monsterIndices[idx]];

        const char* zoneStr = "Far";
        if (entity.Zone == PluginSDK::NearbyZone::InnerCircle) zoneStr = "Inner";
        else if (entity.Zone == PluginSDK::NearbyZone::OuterCircle) zoneStr = "Outer";

        const char* rarityStr = "Normal";
        if (entity.Rarity == 1) rarityStr = "Magic";
        else if (entity.Rarity == 2) rarityStr = "Rare";
        else if (entity.Rarity == 3) rarityStr = "Unique";

        char label[512];
        std::snprintf(label, sizeof(label), "[%s] [%s] %u %s",
            zoneStr, rarityStr, entity.Id, entity.Path.c_str());

        bool selected = ImGui::Selectable(label, (int)idx == selectedIdx);
        if (selected) {
            selectedIdx = (int)idx;
        }
    }

    ImGui::EndChild();
    return selectedIdx;
}

} // namespace AutoReflex