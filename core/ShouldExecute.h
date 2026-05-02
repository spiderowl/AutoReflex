// AutoReflex - ShouldExecute
// Evaluates whether plugin execution should proceed based on game state.
// Phase 2: T17 - blocking checks (town, hideout, dead, grace period, etc.)
//
// SDK API names verified against PluginContext.h (T12):
//   GetSnapshot()              - returns shared_ptr<const PluginGameSnapshot>
//   IsAttached()               - bool
//   IsInGame()                 - bool
//   IsGameForeground()         - bool
//   GetPlayerVitals()          - returns PlayerVitals {HPPercent, Buffs}
//   PluginGameSnapshot::CurrentState   - GameStateTypes enum
//   PluginGameSnapshot::IsTown        - bool
//   PluginGameSnapshot::IsHideout     - bool
//   PluginGameSnapshot::Player        - RadarEntity {CurrentHP, MaxHP}
//   RadarEntity::entityType            - EntityTypes enum (Monster = 5)
//   RadarEntity::Zone                  - NearbyZone enum (InnerCircle=1, OuterCircle=2)
//   RadarEntity::Rarity                - int (0=Normal, 1=Magic, 2=Rare, 3=Unique)
//   RadarEntity::ComponentCache        - EntityComponentCache {HasBuffs()}
//   PluginContext::ReadBuffsComponent  - returns PluginBuffsData {Buffs: vector<PluginBuffData>}
//   PluginBuffData::Name               - std::string
//   PluginGameSnapshot::Entities       - vector<RadarEntity>

#pragma once

#include <string>

struct PluginContext;

namespace AutoReflex {

/// Returns true if the plugin should execute rules right now.
/// If returning false, outReason is set with a human-readable explanation.
bool ShouldExecute(PluginContext* ctx, std::string& outReason);

} // namespace AutoReflex