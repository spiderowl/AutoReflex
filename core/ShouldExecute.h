// AutoReflex - ShouldExecute
// Gate check: are we in a game state where rules are allowed to fire?
//
// Takes the already-fetched snapshot to avoid extra GetSnapshot() calls
// on the hot path. Writes a short reason string into outReason.

#pragma once

#include <string>

struct PluginContext;
namespace PluginSDK { struct PluginGameSnapshot; }

namespace AutoReflex {

/**
 * Determines whether rule evaluation is allowed for the current game state.
 *
 * @param pluginContext Host context used for attachment/foreground checks.
 * @param gameSnapshot Already-fetched snapshot for the current tick.
 * @param outExecutionGateReason Human-readable reason when evaluation is blocked.
 * @returns True when rules may execute; otherwise false.
 */
bool DetermineWhetherRulesShouldExecute(
    PluginContext* pluginContext,
    const PluginSDK::PluginGameSnapshot* gameSnapshot,
    std::string& outExecutionGateReason);

} // namespace AutoReflex
