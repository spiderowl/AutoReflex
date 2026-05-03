// EntityList.cpp - Monster/entity selection list UI implementation

#include "EntityList.h"
#include "../sdk/PluginGameData.h"
#include "../sdk/PluginHelpers.h"
#include "../imgui/imgui.h"

namespace AutoReflex {

std::vector<size_t> CollectMonsters(const std::vector<PluginSDK::DebugEntityInfo>& debugEntities) {
    std::vector<size_t> indices;
    for (size_t i = 0; i < debugEntities.size(); i++) {
        // Use SDK EntityTypes enum instead of hardcoded values
        auto type = static_cast<PluginSDK::EntityTypes>(debugEntities[i].EntityType);
        if (type == PluginSDK::EntityTypes::NPC || type == PluginSDK::EntityTypes::Monster) {
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

        // Use SDK helper functions instead of manual string mapping
        const char* zoneStr = PluginSDK::GetNearbyZoneName(entity.Zone);
        const char* rarityStr = PluginSDK::GetRarityName(entity.Rarity);

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
