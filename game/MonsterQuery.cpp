#include "MonsterQuery.h"
#include <algorithm>
#include <limits>
#include <cmath>

#include <imgui.h>

namespace AutoReflex { namespace Game {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
CachedMonsters::CachedMonsters(const NearbyMonsterCache& cache)
    : cache_(cache)
{
}

// ===================================================================
//  FILTERS — each appends a predicate, returns *this for chaining
// ===================================================================

CachedMonsters& CachedMonsters::WithinRange(float maxDistance)
{
    predicates_.push_back([maxDistance](const MonsterInfo& m) {
        return m.distanceToPlayer <= maxDistance;
    });
    return *this;
}

CachedMonsters& CachedMonsters::NearCursor(float maxDistance)
{
    predicates_.push_back([maxDistance](const MonsterInfo& m) {
        return m.distanceToCursor <= maxDistance;
    });
    return *this;
}

CachedMonsters& CachedMonsters::Hostile()
{
    predicates_.push_back([](const MonsterInfo& m) {
        return m.reaction == 0;
    });
    return *this;
}

CachedMonsters& CachedMonsters::Friendly()
{
    predicates_.push_back([](const MonsterInfo& m) {
        return m.reaction == 2;
    });
    return *this;
}

CachedMonsters& CachedMonsters::Rarity(int rarityMask)
{
    predicates_.push_back([rarityMask](const MonsterInfo& m) {
        if (rarityMask == RarityAny) return true;
        int bit = (1 << m.rarity) & 0xF;
        return (bit & rarityMask) != 0;
    });
    return *this;
}

CachedMonsters& CachedMonsters::PathContains(const std::string& substring)
{
    predicates_.push_back([=](const MonsterInfo& m) {
        return ContainsIgnoreCase(m.path, substring);
    });
    return *this;
}

CachedMonsters& CachedMonsters::HasBuff(const std::string& buffName)
{
    predicates_.push_back([=](const MonsterInfo& m) {
        return m.buffNames.count(buffName) > 0;
    });
    return *this;
}

CachedMonsters& CachedMonsters::NotHasBuff(const std::string& buffName)
{
    predicates_.push_back([=](const MonsterInfo& m) {
        return m.buffNames.count(buffName) == 0;
    });
    return *this;
}

CachedMonsters& CachedMonsters::IsTargeted()
{
    predicates_.push_back([](const MonsterInfo& m) {
        return m.isTargeted;
    });
    return *this;
}

CachedMonsters& CachedMonsters::MinHealthPct(float minPct)
{
    predicates_.push_back([minPct](const MonsterInfo& m) {
        return m.healthPct >= minPct;
    });
    return *this;
}

CachedMonsters& CachedMonsters::MaxHealthPct(float maxPct)
{
    predicates_.push_back([maxPct](const MonsterInfo& m) {
        return m.healthPct <= maxPct;
    });
    return *this;
}

CachedMonsters& CachedMonsters::IsUsingAbility()
{
    predicates_.push_back([](const MonsterInfo& m) {
        return m.isUsingAbility;
    });
    return *this;
}

CachedMonsters& CachedMonsters::Where(MonsterPredicate pred)
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

bool CachedMonsters::Any() const
{
    for (const auto& m : cache_.All()) {
        if (PassesAll(m, predicates_)) return true;
    }
    return false;
}

int CachedMonsters::Count() const
{
    int c = 0;
    for (const auto& m : cache_.All()) {
        if (PassesAll(m, predicates_)) ++c;
    }
    return c;
}

std::optional<MonsterInfo> CachedMonsters::Nearest() const
{
    // Cache is already sorted by distanceToPlayer ascending.
    // First match is the nearest.
    for (const auto& m : cache_.All()) {
        if (PassesAll(m, predicates_)) return m;
    }
    return std::nullopt;
}

std::optional<MonsterInfo> CachedMonsters::NearestToCursor() const
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

std::vector<MonsterInfo> CachedMonsters::All() const
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

// ============================================================================
// Snapshot-based fluent builder implementation
// ============================================================================

MonsterQuery::MonsterQuery(PluginContext* ctx,
                           std::shared_ptr<const PluginSDK::PluginGameSnapshot> snap)
    : m_Ctx(ctx)
    , m_Snap(std::move(snap))
{
}

MonsterQuery& MonsterQuery::WithinRange(float gridUnits) { m_MaxRange = gridUnits; return *this; }
MonsterQuery& MonsterQuery::NearCursor(float pixels) { m_CursorRange = pixels; return *this; }
MonsterQuery& MonsterQuery::InZone(PluginSDK::NearbyZone zone) { m_Zone = zone; return *this; }

MonsterQuery& MonsterQuery::Magic() { return AtLeast(MinRarity::Magic); }
MonsterQuery& MonsterQuery::Rare() { return AtLeast(MinRarity::Rare); }
MonsterQuery& MonsterQuery::Unique() { return AtLeast(MinRarity::Unique); }
MonsterQuery& MonsterQuery::AtLeast(MinRarity rarity) { m_MinRarity = rarity; return *this; }

MonsterQuery& MonsterQuery::WithAllBuffs(std::vector<std::string> buffNames) { m_RequiredBuffs = std::move(buffNames); return *this; }
MonsterQuery& MonsterQuery::WithAnyBuff(std::vector<std::string> buffNames) { m_AnyOfBuffs = std::move(buffNames); return *this; }

MonsterQuery& MonsterQuery::PathContains(std::string needle) { m_PathNeedle = std::move(needle); return *this; }

MonsterQuery& MonsterQuery::IncludeFriendly() { m_HostileOnly = false; return *this; }
MonsterQuery& MonsterQuery::IncludeSleeping() { m_ExcludeSleep = false; return *this; }

MonsterQuery& MonsterQuery::Where(std::function<bool(const PluginSDK::RadarEntity&)> pred) { m_Predicate = std::move(pred); return *this; }

namespace {
    static inline float GridDistToPlayer(const PluginSDK::RadarEntity& e, const PluginSDK::RadarEntity& player) {
        const float dx = e.GridPositionX - player.GridPositionX;
        const float dy = e.GridPositionY - player.GridPositionY;
        return std::sqrtf(dx * dx + dy * dy);
    }

    static bool HasBuffName(const PluginSDK::PluginBuffsData& buffsData, const std::string& name) {
        for (const auto& b : buffsData.Buffs) {
            if (b.Name == name) return true;
        }
        return false;
    }
}

bool MonsterQuery::PassesFilter(const PluginSDK::RadarEntity& e) const
{
    if (e.entityType != PluginSDK::EntityTypes::Monster) return false;
    if (e.entityState == PluginSDK::EntityStates::Useless) return false;
    if (m_HostileOnly && e.entityState == PluginSDK::EntityStates::MonsterFriendly) return false;
    if (m_ExcludeSleep && e.IsSleeping) return false;
    if (e.Rarity < static_cast<int>(m_MinRarity)) return false;
    if (m_Zone.has_value() && e.Zone != m_Zone.value()) return false;

    if (m_MaxRange.has_value()) {
        const float r = m_MaxRange.value();
        if (r <= 60.f && e.Zone != PluginSDK::NearbyZone::InnerCircle) return false;
        if (!m_Snap) return false;
        const float dist = GridDistToPlayer(e, m_Snap->Player);
        if (dist > r) return false;
    }

    if (m_CursorRange.has_value()) {
        if (!m_Ctx || !m_Ctx->WorldToScreen) return false;
        float sx = 0.0f, sy = 0.0f;
        if (!m_Ctx->WorldToScreen(e.WorldX, e.WorldY, e.WorldZ, &sx, &sy)) return false;
        const ImVec2 mouse = ImGui::GetMousePos();
        const float dx = sx - mouse.x;
        const float dy = sy - mouse.y;
        const float dist = std::sqrtf(dx * dx + dy * dy);
        if (dist > m_CursorRange.value()) return false;
    }

    if (!m_PathNeedle.empty()) {
        if (!ContainsIgnoreCase(WStringToString(e.Path), m_PathNeedle)) return false;
    }

    if (!m_RequiredBuffs.empty() || !m_AnyOfBuffs.empty()) {
        if (!m_Ctx || !m_Ctx->ReadBuffsComponent) return false;
        if (!e.ComponentCache.HasBuffs()) return false;
        auto buffsData = m_Ctx->ReadBuffsComponent(e.ComponentCache.BuffsAddr);
        if (!buffsData.Valid) return false;

        if (!m_RequiredBuffs.empty()) {
            for (const auto& req : m_RequiredBuffs) {
                if (!HasBuffName(buffsData, req)) return false;
            }
        }

        if (!m_AnyOfBuffs.empty()) {
            bool any = false;
            for (const auto& name : m_AnyOfBuffs) {
                if (HasBuffName(buffsData, name)) { any = true; break; }
            }
            if (!any) return false;
        }
    }

    if (m_Predicate && !m_Predicate(e)) return false;
    return true;
}

bool MonsterQuery::Any()
{
    if (!m_Snap) return false;
    for (const auto& e : m_Snap->Entities) {
        if (!e.IsValid) continue;
        if (PassesFilter(e)) return true;
    }
    return false;
}

int MonsterQuery::Count()
{
    if (!m_Snap) return 0;
    int c = 0;
    for (const auto& e : m_Snap->Entities) {
        if (!e.IsValid) continue;
        if (PassesFilter(e)) ++c;
    }
    return c;
}

const PluginSDK::RadarEntity* MonsterQuery::Nearest()
{
    if (!m_Snap) return nullptr;
    const PluginSDK::RadarEntity* best = nullptr;
    float bestDist = std::numeric_limits<float>::max();

    for (const auto& e : m_Snap->Entities) {
        if (!e.IsValid) continue;
        if (!PassesFilter(e)) continue;
        const float d = GridDistToPlayer(e, m_Snap->Player);
        if (d < bestDist) { bestDist = d; best = &e; }
    }

    return best;
}

std::vector<const PluginSDK::RadarEntity*> MonsterQuery::All()
{
    std::vector<const PluginSDK::RadarEntity*> out;
    if (!m_Snap) return out;
    out.reserve(m_Snap->Entities.size());
    for (const auto& e : m_Snap->Entities) {
        if (!e.IsValid) continue;
        if (PassesFilter(e)) out.push_back(&e);
    }
    return out;
}

MonsterQuery Monsters(PluginContext* ctx,
                      const std::shared_ptr<const PluginSDK::PluginGameSnapshot>& snap)
{
    return MonsterQuery(ctx, snap);
}

}} // namespace AutoReflex::Game