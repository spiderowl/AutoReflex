#pragma once

#include "MonsterInfo.h"
#include "MonsterHelpers.h"
#include "../sdk/PluginGameData.h"
#include "../sdk/PluginContext.h"

#include <vector>

namespace AutoReflex { namespace Game {

// ---------------------------------------------------------------------------
// NearbyMonsterCache — per-tick cache of monster data.
//
// Rebuild() is called once per frame. It iterates the snapshot's entity list,
// keeps only Monsters, reads their components via PluginContext, and populates
// a sorted std::vector<MonsterInfo>.  The vector is sorted by distanceToPlayer.
//
// This is the cold path (runs once per frame).  MonsterQuery reads the cache
// and is the hot path (runs on every rule evaluation).
// ---------------------------------------------------------------------------
class NearbyMonsterCache {
public:
    // Rebuild the cache from the current snapshot and PluginContext.
    // cursorGridPos is the player cursor position in grid coordinates
    // (computed by the caller using ScreenToGrid).
    void Rebuild(PluginContext* ctx, const PluginSDK::PluginGameSnapshot& snap,
                 Vector2f cursorGridPos);

    // Access the rebuilt monster list (read-only).
    const std::vector<MonsterInfo>& All() const { return monsters_; }

    // Number of monsters in the cache.
    size_t Count() const { return monsters_.size(); }

    // Clear the cache (useful when leaving an area).
    void Clear() { monsters_.clear(); }

private:

    // Convert a single RadarEntity to MonsterInfo using the component cache.
    MonsterInfo BuildMonsterInfo(PluginContext* ctx, const PluginSDK::RadarEntity& e,
                                 float playerGX, float playerGY,
                                 float cursorGX, float cursorGY);

    std::vector<MonsterInfo> monsters_;
};

}} // namespace AutoReflex::Game