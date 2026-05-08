// AutoReflex - MonsterCandidateSelection.cpp

#include "MonsterCandidateSelection.h"

#include <algorithm>
#include <utility>

namespace AutoReflex::Rules {

namespace {

void EnsureCandidateBuffersAreReserved(
    std::vector<std::pair<float, const PluginSDK::RadarEntity*>>& outHostileScoredCandidates,
    std::vector<std::pair<float, const PluginSDK::RadarEntity*>>& outFriendlyScoredCandidates,
    std::vector<const PluginSDK::RadarEntity*>& outHostileMonsterCandidates,
    std::vector<const PluginSDK::RadarEntity*>& outFriendlyMonsterCandidates,
    std::size_t maximumCandidateCount)
{
    static thread_local bool hasReservedCandidateBuffers = false;
    if (hasReservedCandidateBuffers) return;

    outHostileScoredCandidates.reserve(512);
    outFriendlyScoredCandidates.reserve(256);
    outHostileMonsterCandidates.reserve(maximumCandidateCount);
    outFriendlyMonsterCandidates.reserve(maximumCandidateCount);
    hasReservedCandidateBuffers = true;
}

void CopyUpToClosestEntitiesFromScoredList(
    std::vector<std::pair<float, const PluginSDK::RadarEntity*>>& scoredEntitiesByDistanceSquared,
    std::vector<const PluginSDK::RadarEntity*>& outCandidateEntities,
    std::size_t maximumCandidateCount)
{
    outCandidateEntities.clear();
    if (scoredEntitiesByDistanceSquared.empty()) return;

    const std::size_t candidateCountToTake =
        std::min(maximumCandidateCount, scoredEntitiesByDistanceSquared.size());

    // `std::nth_element` requires an iterator in [begin, end); if we take all, skip partitioning.
    if (candidateCountToTake < scoredEntitiesByDistanceSquared.size()) {
        std::nth_element(
            scoredEntitiesByDistanceSquared.begin(),
            scoredEntitiesByDistanceSquared.begin() + candidateCountToTake,
            scoredEntitiesByDistanceSquared.end(),
            [](const auto& left, const auto& right) { return left.first < right.first; });
    }

    outCandidateEntities.reserve(candidateCountToTake);
    for (std::size_t candidateIndex = 0; candidateIndex < candidateCountToTake; ++candidateIndex) {
        outCandidateEntities.push_back(scoredEntitiesByDistanceSquared[candidateIndex].second);
    }
}

} // namespace

bool BuildClosestMonsterCandidateListsForSnapshot(
    const PluginSDK::PluginGameSnapshot* gameSnapshot,
    std::size_t maximumCandidateCount,
    std::vector<const PluginSDK::RadarEntity*>& outHostileMonsterCandidates,
    std::vector<const PluginSDK::RadarEntity*>& outFriendlyMonsterCandidates)
{
    if (!gameSnapshot) return false;

    static thread_local std::vector<std::pair<float, const PluginSDK::RadarEntity*>>
        hostileScoredCandidatesByDistanceSquared;
    static thread_local std::vector<std::pair<float, const PluginSDK::RadarEntity*>>
        friendlyScoredCandidatesByDistanceSquared;

    EnsureCandidateBuffersAreReserved(
        hostileScoredCandidatesByDistanceSquared,
        friendlyScoredCandidatesByDistanceSquared,
        outHostileMonsterCandidates,
        outFriendlyMonsterCandidates,
        maximumCandidateCount);

    hostileScoredCandidatesByDistanceSquared.clear();
    friendlyScoredCandidatesByDistanceSquared.clear();

    const auto& entities = gameSnapshot->Entities;
    const float playerGridPositionX = gameSnapshot->Player.GridPositionX;
    const float playerGridPositionY = gameSnapshot->Player.GridPositionY;

    for (const auto& entity : entities) {
        if (entity.entityType != PluginSDK::EntityTypes::Monster) continue;
        if (!entity.IsValid) continue;
        if (entity.CurrentHP <= 0) continue;
        if (entity.IsSleeping) continue;
        if (!(entity.Zone == PluginSDK::NearbyZone::InnerCircle
           || entity.Zone == PluginSDK::NearbyZone::OuterCircle)) {
            continue;
        }

        const float gridDeltaX = entity.GridPositionX - playerGridPositionX;
        const float gridDeltaY = entity.GridPositionY - playerGridPositionY;
        const float gridDistanceSquared = gridDeltaX * gridDeltaX + gridDeltaY * gridDeltaY;

        if (entity.Reaction == 0) {
            hostileScoredCandidatesByDistanceSquared.emplace_back(gridDistanceSquared, &entity);
        } else if (entity.Reaction == 2) {
            friendlyScoredCandidatesByDistanceSquared.emplace_back(gridDistanceSquared, &entity);
        }
    }

    CopyUpToClosestEntitiesFromScoredList(
        hostileScoredCandidatesByDistanceSquared,
        outHostileMonsterCandidates,
        maximumCandidateCount);
    CopyUpToClosestEntitiesFromScoredList(
        friendlyScoredCandidatesByDistanceSquared,
        outFriendlyMonsterCandidates,
        maximumCandidateCount);

    return true;
}

} // namespace AutoReflex::Rules

