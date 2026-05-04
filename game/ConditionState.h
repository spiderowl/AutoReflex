// AutoReflex - ConditionState
// Tracks game state for rule evaluation.
// Delegates monster data to NearbyMonsterCache (built once per tick).

#pragma once

#include "NearbyMonsterCache.h"
#include "MonsterQuery.h"

#include <vector>

struct PluginContext;
namespace PluginSDK { struct PluginGameSnapshot; }

namespace AutoReflex {
namespace Game {

class ConditionState {
public:
    void Update(PluginContext* ctx, const PluginSDK::PluginGameSnapshot* snapshot,
                Vector2f cursorGridPos);
    void ResetOnAreaChange();

    // Forward to cache
    const NearbyMonsterCache& Cache() const { return cache_; }
    size_t GetMonsterCount() const { return cache_.Count(); }

    // Convenience: count monsters matching a buff
    int CountMonstersWithBuff(const std::string& buffName) const;

    // Convenience: run a Monsters query (syntactic sugar)
    CachedMonsters Query() const { return CachedMonsters(cache_); }

    // Cursor position in grid coordinates (for EXPRTK expressions)
    Vector2f CursorPos() const { return cursorGridPos_; }

private:
    NearbyMonsterCache cache_;
    Vector2f cursorGridPos_; // Cursor position in grid coordinates
};

} // namespace Game
} // namespace AutoReflex