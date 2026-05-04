#include "MonsterQuery.h"
#include <algorithm>
#include <limits>

namespace AutoReflex { namespace Game {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
Monsters::Monsters(const NearbyMonsterCache& cache)
    : cache_(cache)
{
}

// ===================================================================
//  FILTERS — each appends a predicate, returns *this for chaining
// ===================================================================

Monsters& Monsters::WithinRange(float maxDistance)
{
    predicates_.push_back([maxDistance](const MonsterInfo& m) {
        return m.distanceToPlayer <= maxDistance;
    });
    return *this;
}

Monsters& Monsters::NearCursor(float maxDistance)
{
    predicates_.push_back([maxDistance](const MonsterInfo& m) {
        return m.distanceToCursor <= maxDistance;
    });
    return *this;
}

Monsters& Monsters::Hostile()
{
    predicates_.push_back([](const MonsterInfo& m) {
        return m.reaction == 0;
    });
    return *this;
}

Monsters& Monsters::Friendly()
{
    predicates_.push_back([](const MonsterInfo& m) {
        return m.reaction == 2;
    });
    return *this;
}

Monsters& Monsters::Rarity(int rarityMask)
{
    predicates_.push_back([rarityMask](const MonsterInfo& m) {
        if (rarityMask == RarityAny) return true;
        int bit = (1 << m.rarity) & 0xF;
        return (bit & rarityMask) != 0;
    });
    return *this;
}

Monsters& Monsters::PathContains(const std::string& substring)
{
    predicates_.push_back([=](const MonsterInfo& m) {
        return ContainsIgnoreCase(m.path, substring);
    });
    return *this;
}

Monsters& Monsters::HasBuff(const std::string& buffName)
{
    predicates_.push_back([=](const MonsterInfo& m) {
        return m.buffNames.count(buffName) > 0;
    });
    return *this;
}

Monsters& Monsters::NotHasBuff(const std::string& buffName)
{
    predicates_.push_back([=](const MonsterInfo& m) {
        return m.buffNames.count(buffName) == 0;
    });
    return *this;
}

Monsters& Monsters::IsTargeted()
{
    predicates_.push_back([](const MonsterInfo& m) {
        return m.isTargeted;
    });
    return *this;
}

Monsters& Monsters::MinHealthPct(float minPct)
{
    predicates_.push_back([minPct](const MonsterInfo& m) {
        return m.healthPct >= minPct;
    });
    return *this;
}

Monsters& Monsters::MaxHealthPct(float maxPct)
{
    predicates_.push_back([maxPct](const MonsterInfo& m) {
        return m.healthPct <= maxPct;
    });
    return *this;
}

Monsters& Monsters::IsUsingAbility()
{
    predicates_.push_back([](const MonsterInfo& m) {
        return m.isUsingAbility;
    });
    return *this;
}

Monsters& Monsters::Where(MonsterPredicate pred)
{
    predicates_.push_back(std::move(pred));
    return *this;
}

// ===================================================================
//  INTERNAL — check if a monster passes ALL predicates
// ===================================================================

namespace {
    inline bool PassesAll(const MonsterInfo& m,
                          const std::vector<MonsterPredicate>& preds)
    {
        for (const auto& p : preds) {
            if (!p(m)) return false;
        }
        return true;
    }
}

// ===================================================================
//  TERMINALS — evaluate the query against the cache
// ===================================================================

bool Monsters::Any() const
{
    for (const auto& m : cache_.All()) {
        if (PassesAll(m, predicates_)) return true;
    }
    return false;
}

int Monsters::Count() const
{
    int c = 0;
    for (const auto& m : cache_.All()) {
        if (PassesAll(m, predicates_)) ++c;
    }
    return c;
}

std::optional<MonsterInfo> Monsters::Nearest() const
{
    // Cache is already sorted by distanceToPlayer ascending.
    // First match is the nearest.
    for (const auto& m : cache_.All()) {
        if (PassesAll(m, predicates_)) return m;
    }
    return std::nullopt;
}

std::optional<MonsterInfo> Monsters::NearestToCursor() const
{
    MonsterInfo best;
    float bestDist = std::numeric_limits<float>::max();
    bool found = false;

    for (const auto& m : cache_.All()) {
        if (!PassesAll(m, predicates_)) continue;
        if (m.distanceToCursor < bestDist) {
            bestDist = m.distanceToCursor;
            best = m;
            found = true;
        }
    }
    return found ? std::optional<MonsterInfo>(best) : std::nullopt;
}

std::vector<MonsterInfo> Monsters::All() const
{
    std::vector<MonsterInfo> result;
    const auto& cacheAll = cache_.All();
    result.reserve(cacheAll.size());

    for (const auto& m : cacheAll) {
        if (PassesAll(m, predicates_)) {
            result.push_back(m);
        }
    }
    return result;
}

}} // namespace AutoReflex::Game