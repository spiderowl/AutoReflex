#include "NearbyMonsterCache.h"
#include <algorithm>

namespace AutoReflex { namespace Game {

// ---------------------------------------------------------------------------
// Rebuild — called once per frame (cold path)
// ---------------------------------------------------------------------------
void NearbyMonsterCache::Rebuild(PluginContext* ctx,
                                 const PluginSDK::PluginGameSnapshot& snap,
                                 Vector2f cursorGridPos)
{
    if (!ctx || !ctx->GetSnapshot) return;

    monsters_.clear();

    // Player grid position from snapshot
    float playerGX = snap.Player.GridPositionX;
    float playerGY = snap.Player.GridPositionY;

    // Reserve space to avoid reallocations
    monsters_.reserve(snap.Entities.size());

    for (const auto& e : snap.Entities) {
        if (e.entityType != PluginSDK::EntityTypes::Monster) continue;
        if (!e.IsValid) continue;

        monsters_.push_back(BuildMonsterInfo(ctx, e, playerGX, playerGY,
                                             cursorGridPos.x, cursorGridPos.y));
    }

    // Sort by distance to player (ascending)
    std::sort(monsters_.begin(), monsters_.end(),
              [](const MonsterInfo& a, const MonsterInfo& b) {
                  return a.distanceToPlayer < b.distanceToPlayer;
              });
}

// ---------------------------------------------------------------------------
// BuildMonsterInfo — read components for a single monster
// ---------------------------------------------------------------------------
MonsterInfo NearbyMonsterCache::BuildMonsterInfo(PluginContext* ctx,
                                                  const PluginSDK::RadarEntity& e,
                                                  float playerGX, float playerGY,
                                                  float cursorGX, float cursorGY)
{
    MonsterInfo m;
    m.id           = e.Id;
    m.gridX        = e.GridPositionX;
    m.gridY        = e.GridPositionY;
    m.rarity       = e.Rarity;
    m.reaction     = e.Reaction;
    m.path         = WStringToString(e.Path);

    // Distances
    m.distanceToPlayer = GridDistance(m.gridX, m.gridY, playerGX, playerGY);
    m.distanceToCursor = GridDistance(m.gridX, m.gridY, cursorGX, cursorGY);

    const auto& cc = e.ComponentCache;

    // --- Life component (HP / healthPct) ---
    if (cc.HasLife() && ctx->ReadLifeComponent) {
        auto life = ctx->ReadLifeComponent(cc.LifeAddr);
        if (life.Valid && life.Health.Total > 0) {
            m.hp      = static_cast<float>(life.Health.Current);
            m.maxHp   = static_cast<float>(life.Health.Total);
            m.healthPct = m.hp / m.maxHp;
        }
    }

    // --- Targetable component (isTargeted, isTargetable) ---
    if (cc.HasTargetable() && ctx->ReadTargetableComponent) {
        auto tgt = ctx->ReadTargetableComponent(cc.TargetableAddr);
        if (tgt.Valid) {
            m.isTargetable = tgt.IsTargetable;
            m.isTargeted   = tgt.IsTargettedByPlayer;
        }
    }

    // --- Actor component (isUsingAbility) ---
    if (cc.HasActor() && ctx->ReadActorComponent) {
        auto actor = ctx->ReadActorComponent(cc.ActorAddr);
        if (actor.Valid) {
            m.isUsingAbility = !actor.ActiveSkills.empty();
        }
    }

    // --- Buffs component (buffNames) ---
    if (cc.HasBuffs() && ctx->ReadBuffsComponent) {
        auto buffs = ctx->ReadBuffsComponent(cc.BuffsAddr);
        if (buffs.Valid) {
            for (const auto& b : buffs.Buffs) {
                m.buffNames.insert(b.Name);
            }
        }
    }

    return m;
}

}} // namespace AutoReflex::Game