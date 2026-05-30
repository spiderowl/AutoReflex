// AutoReflex - MonsterCandidateSelection.cpp

#include "MonsterCandidateSelection.h"

#include <algorithm>
#include <utility>

namespace AutoReflex::Rules {

namespace {

void EnsureCandidateBuffersAreReserved(
    std::vector<std::pair<float, const PluginSDK::Entity*>>& outHostileScoredCandidates,
    std::vector<std::pair<float, const PluginSDK::Entity*>>& outFriendlyScoredCandidates,
    std::vector<const PluginSDK::Entity*>& outHostileMonsterCandidates,
    std::vector<const PluginSDK::Entity*>& outFriendlyMonsterCandidates,
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
    std::vector<std::pair<float, const PluginSDK::Entity*>>& scoredEntitiesByDistanceSquared,
    std::vector<const PluginSDK::Entity*>& outCandidateEntities,
    std::size_t maximumCandidateCount)
{
    outCandidateEntities.clear();
    if (scoredEntitiesByDistanceSquared.empty()) return;

    const std::size_t candidateCountToTake =
        std::min(maximumCandidateCount, scoredEntitiesByDistanceSquared.size());

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
    const PluginSDK::Snapshot& gameSnapshot,
    std::size_t maximumCandidateCount,
    bool buildHostileCandidates,
    bool buildFriendlyCandidates,
    std::vector<const PluginSDK::Entity*>& outHostileMonsterCandidates,
    std::vector<const PluginSDK::Entity*>& outFriendlyMonsterCandidates)
{
    if (!buildHostileCandidates) outHostileMonsterCandidates.clear();
    if (!buildFriendlyCandidates) outFriendlyMonsterCandidates.clear();
    if (!buildHostileCandidates && !buildFriendlyCandidates) return true;
    static thread_local std::vector<std::pair<float, const PluginSDK::Entity*>>
        hostileScoredCandidatesByDistanceSquared;
    static thread_local std::vector<std::pair<float, const PluginSDK::Entity*>>
        friendlyScoredCandidatesByDistanceSquared;

    EnsureCandidateBuffersAreReserved(
        hostileScoredCandidatesByDistanceSquared,
        friendlyScoredCandidatesByDistanceSquared,
        outHostileMonsterCandidates,
        outFriendlyMonsterCandidates,
        maximumCandidateCount);

    hostileScoredCandidatesByDistanceSquared.clear();
    friendlyScoredCandidatesByDistanceSquared.clear();

    const auto& entities = gameSnapshot.Entities;
    const float playerGridPositionX = gameSnapshot.Player.GridPositionX;
    const float playerGridPositionY = gameSnapshot.Player.GridPositionY;

    for (const auto& entity : entities) {
        if (entity.EntityType != PluginSDK::EntityType::Monster) continue;
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
            if (buildHostileCandidates) {
                hostileScoredCandidatesByDistanceSquared.emplace_back(gridDistanceSquared, &entity);
            }
        } else if (entity.Reaction == 2) {
            if (buildFriendlyCandidates) {
                friendlyScoredCandidatesByDistanceSquared.emplace_back(gridDistanceSquared, &entity);
            }
        }
    }

    if (buildHostileCandidates) {
        CopyUpToClosestEntitiesFromScoredList(
            hostileScoredCandidatesByDistanceSquared,
            outHostileMonsterCandidates,
            maximumCandidateCount);
    }
    if (buildFriendlyCandidates) {
        CopyUpToClosestEntitiesFromScoredList(
            friendlyScoredCandidatesByDistanceSquared,
            outFriendlyMonsterCandidates,
            maximumCandidateCount);
    }

    return true;
}

} // namespace AutoReflex::Rules
