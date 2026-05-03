// AutoReflex - MonsterInfo
// Thin wrapper around PluginSDK::RadarEntity for monster utilities
// Uses SDK types directly instead of custom MonsterData/BuffInfo

#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "sdk/PluginGameData.h"

namespace AutoReflex {
namespace Game {

// Parse monster data from the game snapshot
// Returns vector of RadarEntities filtered to monsters only
std::vector<PluginSDK::RadarEntity> ParseMonsters(
    void* context,
    const void* snapshot);

// Extract buffs for a single entity from its component data
std::vector<PluginSDK::PluginBuffData> ExtractBuffs(
    void* context,
    uintptr_t entityAddress);

// Check if an entity path represents a monster/enemy
bool IsMonsterEntity(const std::string& path);

// Convenience: check if a RadarEntity is a monster
inline bool IsMonster(const PluginSDK::RadarEntity& e) {
    return e.entityType == PluginSDK::EntityTypes::Monster ||
           e.entityType == PluginSDK::EntityTypes::NPC;
}

// Convenience: health percentage from RadarEntity
inline float HealthPercent(const PluginSDK::RadarEntity& e) {
    return e.MaxHP > 0 ? (float)e.CurrentHP / (float)e.MaxHP * 100.0f : 0.0f;
}

// Convenience: is entity alive
inline bool IsAlive(const PluginSDK::RadarEntity& e) {
    return e.CurrentHP > 0 && !e.IsSleeping;
}

} // namespace Game
} // namespace AutoReflex