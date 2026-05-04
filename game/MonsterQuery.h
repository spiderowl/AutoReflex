#pragma once

#include "NearbyMonsterCache.h"
#include "MonsterHelpers.h"

#include <functional>
#include <vector>
#include <optional>
#include <string>
#include <memory>

namespace AutoReflex { namespace Game {

// ---------------------------------------------------------------------------
// CachedMonsters — fluent builder over NearbyMonsterCache.
//
// Usage:
//   auto cache = NearbyMonsterCache();
//   cache.Rebuild(ctx, snap, cursorGrid);
//
//   if (CachedMonsters(cache).WithinRange(50).Hostile().Any()) { ... }
//   auto nearest = CachedMonsters(cache).WithinRange(100).Nearest();
//   int cnt = CachedMonsters(cache).Rarity(MonsterRarityFlag::RarityUnique).Count();
//
// All filtering is lazy — the cache is scanned only when a terminal
// (Any, Count, Nearest, NearestToCursor, All) is called.
//
// The predicates are stored as lightweight values/pointers so the chain
// itself has zero heap allocations.  The .All() terminal returns a vector.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Predicate storage — kept as thin pointers to avoid virtual dispatch
// overhead on the hot path.  Each filter mutates the internal predicate list.
// ---------------------------------------------------------------------------
using MonsterPredicate = std::function<bool(const MonsterInfo&)>;

// ---------------------------------------------------------------------------
// CachedMonsters — fluent query builder
// ---------------------------------------------------------------------------
class CachedMonsters {
public:
    // Entry point: construct from a cache reference.
    explicit CachedMonsters(const NearbyMonsterCache& cache);

    // ---- Range filters ----

    // Distance to player (in grid units)
    CachedMonsters& WithinRange(float maxDistance);

    // Distance to cursor (in grid units)
    CachedMonsters& NearCursor(float maxDistance);

    // ---- Reaction filters ----

    // reaction == 0 (Hostile)
    CachedMonsters& Hostile();

    // reaction == 2 (Friendly)
    CachedMonsters& Friendly();

    // ---- Rarity filter ----
    // rarityMask is a bitmask: RarityNormal|RarityMagic|RarityRare|RarityUnique
    // Only monsters whose (1 << rarity) matches a bit in mask are kept.
    CachedMonsters& Rarity(int rarityMask);

    // ---- Path filter ----
    // Keep monsters whose path contains the substring (case-insensitive).
    CachedMonsters& PathContains(const std::string& substring);

    // ---- Buff filters ----

    // Keep monsters that have at least one of the listed buff names.
    CachedMonsters& HasBuff(const std::string& buffName);

    // Keep monsters that have NONE of the listed buff names.
    // (Stack call: .NotHasBuff("immortal").NotHasBuff("shield"))
    CachedMonsters& NotHasBuff(const std::string& buffName);

    // ---- Target filter ----
    CachedMonsters& IsTargeted();

    // ---- Health filters ----

    CachedMonsters& MinHealthPct(float minPct);   // healthPct >= minPct
    CachedMonsters& MaxHealthPct(float maxPct);   // healthPct <= maxPct

    // ---- Ability filter ----
    CachedMonsters& IsUsingAbility();

    // ---- Custom predicate ----
    CachedMonsters& Where(MonsterPredicate pred);

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

// ============================================================================
// Snapshot-based fluent builder (requested API)
// ============================================================================

enum class MinRarity : uint8_t {
    Any    = 0,
    Magic  = 1,
    Rare   = 2,
    Unique = 3,
};

class MonsterQuery {
public:
    MonsterQuery(PluginContext* ctx,
                 std::shared_ptr<const PluginSDK::PluginGameSnapshot> snap);

    // Builder methods
    MonsterQuery& WithinRange(float gridUnits);
    MonsterQuery& NearCursor(float pixels);
    MonsterQuery& InZone(PluginSDK::NearbyZone zone);

    MonsterQuery& Magic();
    MonsterQuery& Rare();
    MonsterQuery& Unique();
    MonsterQuery& AtLeast(MinRarity rarity);

    MonsterQuery& WithAllBuffs(std::vector<std::string> buffNames);
    MonsterQuery& WithAnyBuff(std::vector<std::string> buffNames);

    // Path substring match (case-insensitive). Uses `RadarEntity.Path` only.
    MonsterQuery& PathContains(std::string needle);

    MonsterQuery& IncludeFriendly();
    MonsterQuery& IncludeSleeping();

    MonsterQuery& Where(std::function<bool(const PluginSDK::RadarEntity&)> pred);

    // Terminal methods
    bool Any();
    int Count();
    const PluginSDK::RadarEntity* Nearest();
    std::vector<const PluginSDK::RadarEntity*> All();

private:
    PluginContext*                              m_Ctx;
    std::shared_ptr<const PluginSDK::PluginGameSnapshot>  m_Snap;

    // Filter state
    std::optional<float>                       m_MaxRange;
    std::optional<float>                       m_CursorRange;
    std::optional<PluginSDK::NearbyZone>       m_Zone;
    MinRarity                                  m_MinRarity    = MinRarity::Any;
    std::vector<std::string>                   m_RequiredBuffs;
    std::vector<std::string>                   m_AnyOfBuffs;
    std::string                                m_PathNeedle;
    bool                                       m_HostileOnly  = true;
    bool                                       m_ExcludeSleep = true;
    std::function<bool(const PluginSDK::RadarEntity&)>    m_Predicate;

    bool PassesFilter(const PluginSDK::RadarEntity& e) const;
};

MonsterQuery Monsters(PluginContext* ctx,
                      const std::shared_ptr<const PluginSDK::PluginGameSnapshot>& snap);

}} // namespace AutoReflex::Game