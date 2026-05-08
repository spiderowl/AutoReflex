// AutoReflex - MonsterCandidateSelection
// Shared snapshot scan to build small monster candidate lists for rule evaluation.

#pragma once

#include "../sdk/PluginGameData.h"

#include <cstddef>
#include <vector>

namespace AutoReflex::Rules {

/**
 * Builds per-tick monster candidate lists for hostile and friendly evaluation roots.
 *
 * @param gameSnapshot Snapshot to scan for monster entities.
 * @param maximumCandidateCount Maximum number of closest monsters kept per list.
 * @param outHostileMonsterCandidates Output vector receiving pointers to hostile monsters.
 * @param outFriendlyMonsterCandidates Output vector receiving pointers to friendly monsters.
 * @returns True when candidate lists were built; false when input was invalid.
 */
bool BuildClosestMonsterCandidateListsForSnapshot(
    const PluginSDK::PluginGameSnapshot* gameSnapshot,
    std::size_t maximumCandidateCount,
    std::vector<const PluginSDK::RadarEntity*>& outHostileMonsterCandidates,
    std::vector<const PluginSDK::RadarEntity*>& outFriendlyMonsterCandidates);

} // namespace AutoReflex::Rules

