// AutoReflex - MonsterInfo
// Data structures for monster and buff information

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace AutoReflex {
namespace Game {

// Individual buff on a monster
struct BuffInfo {
    std::string Name;
    int Duration = 0;
    bool IsBeneficial = false;
    bool IsDebuff = false;
};

// Single monster entity data
struct MonsterData {
    uintptr_t Address = 0;
    uint32_t EntityId = 0;
    std::string Path;               // Entity metadata path
    float WorldX = 0, WorldY = 0, WorldZ = 0;
    float ScreenX = 0, ScreenY = 0;
    bool OnScreen = false;
    float HealthPercent = 100.0f;
    int CurrentHP = 0;
    int MaxHP = 0;
    std::vector<BuffInfo> Buffs;
    bool IsAlive = true;
    float DistanceToPlayer = 0.0f;
};

// Parse monster data from the game snapshot
// Returns vector of monsters in the current area
std::vector<MonsterData> ParseMonsters(
    void* context,
    const void* snapshot);

// Extract buffs for a single monster from its component data
std::vector<BuffInfo> ExtractBuffs(
    void* context,
    uintptr_t entityAddress);

// Check if an entity path represents a monster/enemy
bool IsMonsterEntity(const std::string& path);

} // namespace Game
} // namespace AutoReflex