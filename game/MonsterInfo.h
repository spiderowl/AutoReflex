#pragma once

#include <string>
#include <unordered_set>
#include <cstdint>

namespace AutoReflex { namespace Game {

// ---------------------------------------------------------------------------
// MonsterInfo — flat, pre-resolved struct built once per tick per entity.
// All expensive reads (components, path) happen in NearbyMonsterCache,
// not in the query. This is the hot path.
// ---------------------------------------------------------------------------
struct MonsterInfo {
    uint32_t    id               = 0;

    // Grid-space position (same coordinate system as player grid pos)
    float       gridX            = 0.0f;
    float       gridY            = 0.0f;

    // Distances (computed during cache build)
    float       distanceToPlayer = 0.0f;   // euclidean on grid coords
    float       distanceToCursor = 0.0f;   // euclidean on grid coords

    // Entity classification
    int         rarity           = 0;      // 0=Normal 1=Magic 2=Rare 3=Unique
    uint8_t     reaction         = 0;      // 0=Hostile 1=Neutral 2=Friendly

    // Path string from RadarEntity.Path (e.g. "Enemies/Unique/...")
    std::string path;

    // Health (from Life component)
    float       healthPct        = 0.0f;   // 0.0 - 1.0
    float       hp               = 0.0f;
    float       maxHp            = 0.0f;

    // Targetable component
    bool        isTargeted       = false;
    bool        isTargetable     = false;

    // Actor component
    bool        isUsingAbility   = false;

    // Buffs (from Buffs component, stored as unordered_set for O(1) lookup)
    std::unordered_set<std::string> buffNames;
};

}} // namespace AutoReflex::Game