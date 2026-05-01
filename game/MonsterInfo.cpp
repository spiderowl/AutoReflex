// AutoReflex - MonsterInfo.cpp
// Implementation: parse monsters from game snapshot
// Phase 1: Stub implementation matching actual SDK v2 API

#include "MonsterInfo.h"
#include "sdk/PluginGameData.h"
#include "sdk/PluginHelpers.h"
#include <cmath>

namespace AutoReflex {
namespace Game {

std::vector<MonsterData> ParseMonsters(void* context, const void* snapshot) {
    (void)context;
    std::vector<MonsterData> result;

    auto* snap = static_cast<const PluginSDK::PluginGameSnapshot*>(snapshot);
    if (!snap || !snap->IsAttached) return result;

    // Iterate through Entities in the snapshot (actual SDK field name)
    for (const auto& entity : snap->Entities) {
        std::string path = PluginSDK::WideToNarrow(entity.Path);
        if (!IsMonsterEntity(path)) continue;

        MonsterData data;
        data.EntityId = entity.Id;
        data.Path = std::move(path);
        data.WorldX = entity.WorldX;
        data.WorldY = entity.WorldY;
        data.WorldZ = entity.WorldZ;

        // Health from RadarEntity directly
        data.CurrentHP = entity.CurrentHP;
        data.MaxHP = entity.MaxHP;
        data.IsAlive = entity.CurrentHP > 0;
        data.HealthPercent = data.MaxHP > 0
            ? static_cast<float>(data.CurrentHP) * 100.0f / data.MaxHP
            : 0.0f;

        // Distance to player
        float dx = data.WorldX - snap->Player.WorldX;
        float dy = data.WorldY - snap->Player.WorldY;
        float dz = data.WorldZ - snap->Player.WorldZ;
        data.DistanceToPlayer = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (data.IsAlive) {
            result.push_back(std::move(data));
        }
    }

    return result;
}

std::vector<BuffInfo> ExtractBuffs(void*, uintptr_t) {
    // Phase 1: buff extraction stub
    // SDK provides buff data through PlayerVitals.Buffs for the player only.
    // Monster buff reading requires component traversal not available in Phase 1.
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