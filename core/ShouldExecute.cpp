// AutoReflex - ShouldExecute.cpp

#include "ShouldExecute.h"
#include "sdk/PluginContext.h"
#include "sdk/PluginGameData.h"

namespace AutoReflex {

bool DetermineWhetherRulesShouldExecute(
    PluginContext* pluginContext,
    const PluginSDK::PluginGameSnapshot* gameSnapshot,
    std::string& outExecutionGateReason)
{
    if (!pluginContext) {
        outExecutionGateReason = "No context";
        return false;
    }

    if (!pluginContext->IsAttached || !pluginContext->IsAttached()) {
        outExecutionGateReason = "Not attached";
        return false;
    }

    if (!pluginContext->IsInGame || !pluginContext->IsInGame()) {
        outExecutionGateReason = "Not in game";
        return false;
    }

    if (!pluginContext->IsGameForeground || !pluginContext->IsGameForeground()) {
        outExecutionGateReason = "Game not foreground";
        return false;
    }

    if (!gameSnapshot) {
        outExecutionGateReason = "No snapshot";
        return false;
    }

    if (gameSnapshot->IsTown) {
        outExecutionGateReason = "In town";
        return false;
    }

    if (gameSnapshot->IsHideout) {
        outExecutionGateReason = "In hideout";
        return false;
    }

    // Use the vitals already on the snapshot — same data the host would hand
    // back via GetPlayerVitals(), but without the extra bridge call.
    const auto& playerVitals = gameSnapshot->Vitals;
    if (playerVitals.HPPercent <= 0) {
        outExecutionGateReason = "Player dead";
        return false;
    }

    for (const auto& playerBuff : playerVitals.Buffs) {
        if (playerBuff.Name == "grace_period") {
            outExecutionGateReason = "Grace period";
            return false;
        }
    }

    outExecutionGateReason = "Active";
    return true;
}

} // namespace AutoReflex
