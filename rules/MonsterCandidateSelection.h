// AutoReflex - MonsterCandidateSelection.h

#pragma once

#include "../sdk/PluginSDK.h"

#include <cstddef>
#include <vector>

namespace AutoReflex::Rules {

bool BuildClosestMonsterCandidateListsForSnapshot(
    const PluginSDK::Snapshot& gameSnapshot,
    std::size_t maximumCandidateCount,
    bool buildHostileCandidates,
    bool buildFriendlyCandidates,
    std::vector<const PluginSDK::Entity*>& outHostileMonsterCandidates,
    std::vector<const PluginSDK::Entity*>& outFriendlyMonsterCandidates);

} // namespace AutoReflex::Rules
