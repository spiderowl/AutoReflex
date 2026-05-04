#pragma once

#include "NearbyMonsterCache.h"
#include "MonsterHelpers.h"

#include <functional>
#include <vector>
#include <optional>

namespace AutoReflex { namespace Game {

// ---------------------------------------------------------------------------
// MonsterQuery — fluent builder over NearbyMonsterCache.
//
// Usage:
//   auto cache = NearbyMonsterCache();
//   cache.Rebuild(ctx, snap, cursorGrid);
//
//   if (Monsters(cache).WithinRange(50).Hostile().Any()) { ... }
//   auto nearest = Monsters(cache).WithinRange(100).Nearest();
//   int cnt = Monsters(cache).Rarity(MonsterRarityFlag::RarityUnique).Count();
//
// All filtering is lazy — the cache is scanned only when a terminal
// (Any, Count, Nearest, NearestToCursor, All) is called.
//
// The predicates are stored as lightweight values/pointers so the chain
// itself has zero heap allocations.  The .All() terminal returns a vector.
// ---------------------------------------------------------------------------

// Forward declaration
class Monsters;

// ---------------------------------------------------------------------------
// Predicate storage — kept as thin pointers to avoid virtual dispatch
// overhead on the hot path.  Each filter mutates the internal predicate list.
// ---------------------------------------------------------------------------
using MonsterPredicate = std::function<bool(const MonsterInfo&)>;

// ---------------------------------------------------------------------------
// Monsters — fluent query builder
// ---------------------------------------------------------------------------
class Monsters {
public:
    // Entry point: construct from a cache reference.
    explicit Monsters(const NearbyMonsterCache& cache);

    // ---- Range filters ----

    // Distance to player (in grid units)
    Monsters& WithinRange(float maxDistance);

    // Distance to cursor (in grid units)
    Monsters& NearCursor(float maxDistance);

    // ---- Reaction filters ----

    // reaction == 0 (Hostile)
    Monsters& Hostile();

    // reaction == 2 (Friendly)
    Monsters& Friendly();

    // ---- Rarity filter ----
    // rarityMask is a bitmask: RarityNormal|RarityMagic|RarityRare|RarityUnique
    // Only monsters whose (1 << rarity) matches a bit in mask are kept.
    Monsters& Rarity(int rarityMask);

    // ---- Path filter ----
    // Keep monsters whose path contains the substring (case-insensitive).
    Monsters& PathContains(const std::string& substring);

    // ---- Buff filters ----

    // Keep monsters that have at least one of the listed buff names.
    Monsters& HasBuff(const std::string& buffName);

    // Keep monsters that have NONE of the listed buff names.
    // (Stack call: .NotHasBuff("immortal").NotHasBuff("shield"))
    Monsters& NotHasBuff(const std::string& buffName);

    // ---- Target filter ----
    Monsters& IsTargeted();

    // ---- Health filters ----

    Monsters& MinHealthPct(float minPct);   // healthPct >= minPct
    Monsters& MaxHealthPct(float maxPct);   // healthPct <= maxPct

    // ---- Ability filter ----
    Monsters& IsUsingAbility();

    // ---- Custom predicate ----
    Monsters& Where(MonsterPredicate pred);

    // ---- Terminals ----

    // Does at least one monster pass? (short-circuits on first match)
    bool Any() const;

    // Count of monsters that pass all filters.
    int Count() const;

    // Nearest passing monster to player (by distanceToPlayer).
    // Returns empty if no monster passes.
    std::optional<MonsterInfo> Nearest() const;

    // Nearest passing monster to cursor (by distanceToCursor).
    std::optional<MonsterInfo> NearestToCursor() const;

    // All passing monsters (sorted by distanceToPlayer — cache order).
    std::vector<MonsterInfo> All() const;

private:
    const NearbyMonsterCache& cache_;
    std::vector<MonsterPredicate> predicates_;
};

}} // namespace AutoReflex::Game