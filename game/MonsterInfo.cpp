// AutoReflex - MonsterInfo.cpp
// Implementation: parse monsters from game snapshot
// Returns PluginSDK::RadarEntity directly (no custom MonsterData)

#include "MonsterInfo.h"
#include "sdk/PluginGameData.h"
#include "sdk/PluginHelpers.h"
#include <cmath>

namespace AutoReflex {
namespace Game {

std::vector<PluginSDK::RadarEntity> ParseMonsters(void* context, const void* snapshot) {
    (void)context;
    std::vector<PluginSDK::RadarEntity> result;

    auto* snap = static_cast<const PluginSDK::PluginGameSnapshot*>(snapshot);
    if (!snap || !snap->IsAttached) return result;

    // Iterate through Entities in the snapshot
    for (const auto& entity : snap->Entities) {
        std::string path = PluginSDK::WideToNarrow(entity.Path);
        if (!IsMonsterEntity(path)) continue;

        if (entity.CurrentHP <= 0 || entity.IsSleeping) continue;

        result.push_back(entity);
    }

    return result;
}

std::vector<PluginSDK::PluginBuffData> ExtractBuffs(void*, uintptr_t) {
    // Stub: buff extraction via component traversal
    return {};
}

bool IsMonsterEntity(const std::string& path) {
    if (path.empty()) return false;

    // Filter: must be in Monsters/ area and not a boss/miniroom
    if (path.find("Monsters/") == 0) {
        if (path.find("Boss") != std::string::npos) return false;
        if (path.find("Miniroom") != std::string::npos) return false;
        if (path.find("RaidBoss") != std::string::npos) return false;
        return true;
    }

    return false;
}

} // namespace Game
} // namespace AutoReflex