// AutoReflex - MonsterInfo.cpp
// Implementation: parse monsters from game snapshot

#include "MonsterInfo.h"
#include "sdk/PluginContext.h"
#include "sdk/PluginGameData.h"
#include "sdk/PluginHelpers.h"

namespace AutoReflex {
namespace Game {

std::vector<MonsterData> ParseMonsters(void* context, const void* snapshot) {
    std::vector<MonsterData> result;
    
    auto* ctx = static_cast<PluginContext*>(context);
    auto* snap = static_cast<const PluginSDK::PluginGameSnapshot*>(snapshot);
    
    if (!ctx || !snap || !snap->IsAttached) return result;

    // Iterate through nearby entities in the snapshot
    for (const auto& entity : snap->NearbyEntities) {
        if (!IsMonsterEntity(PluginSDK::WideToNarrow(entity.Path))) continue;
        
        MonsterData data;
        data.Address = entity.Address;
        data.EntityId = entity.Id;
        data.Path = PluginSDK::WideToNarrow(entity.Path);
        data.WorldX = entity.WorldX;
        data.WorldY = entity.WorldY;
        data.WorldZ = entity.WorldZ;
        
        // WorldToScreen
        if (ctx->WorldToScreen) {
            float sx = 0, sy = 0;
            data.OnScreen = ctx->WorldToScreen(data.WorldX, data.WorldY, data.WorldZ, &sx, &sy);
            data.ScreenX = sx;
            data.ScreenY = sy;
        }
        
        // Read health from Life component
        if (entity.LifeAddr > 0 && ctx->ReadLifeComponent) {
            auto life = ctx->ReadLifeComponent(entity.LifeAddr);
            if (life.Valid) {
                data.CurrentHP = life.Health.Current;
                data.MaxHP = life.Health.Total;
                data.HealthPercent = life.Health.Total > 0 
                    ? static_cast<float>(life.Health.Current) * 100.0f / life.Health.Total 
                    : 0.0f;
                data.IsAlive = life.Health.Current > 0;
            }
        }
        
        // Calculate distance to player
        float dx = data.WorldX - snap->Player.WorldX;
        float dy = data.WorldY - snap->Player.WorldY;
        float dz = data.WorldZ - snap->Player.WorldZ;
        data.DistanceToPlayer = std::sqrt(dx * dx + dy * dy + dz * dz);
        
        // Extract buffs
        data.Buffs = ExtractBuffs(ctx, entity.Address);
        
        if (data.IsAlive) {
            result.push_back(std::move(data));
        }
    }
    
    return result;
}

std::vector<BuffInfo> ExtractBuffs(void* context, uintptr_t entityAddress) {
    std::vector<BuffInfo> result;
    
    auto* ctx = static_cast<PluginContext*>(context);
    if (!ctx || entityAddress == 0) return result;
    
    // Buffs are read from the entity's Buffs component via the snapshot
    // This is populated during ParseMonsters from the NearbyEntities data
    // For now, use the snapshot's buff reading capability
    
    if (ctx->ReadBuffsComponent) {
        // The BuffsComponent address would need to be obtained from the entity's component list
        // This requires traversing the entity's component array
        // Placeholder: will be populated when component addresses are available
    }
    
    return result;
}

bool IsMonsterEntity(const std::string& path) {
    if (path.empty()) return false;
    
    // Filter: must be in Monsters/ area and not a boss/miniroom
    // Valid paths look like: "Monsters/AreaName/MonsterType"
    if (path.find("Monsters/") == 0) {
        // Exclude bosses and special entities
        if (path.find("Boss") != std::string::npos) return false;
        if (path.find("Miniroom") != std::string::npos) return false;
        if (path.find("RaidBoss") != std::string::npos) return false;
        return true;
    }
    
    return false;
}

} // namespace Game
} // namespace AutoReflex