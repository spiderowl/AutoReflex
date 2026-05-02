// AutoReflex - ShouldExecute.cpp
// Implementation: blocking checks for rule execution
// T17: Basic blocking checks (attached, in-game, foreground, town, hideout)
// T18: Player death check
// T19: Grace period check

#include "ShouldExecute.h"
#include "sdk/PluginContext.h"

namespace AutoReflex {

bool ShouldExecute(PluginContext* ctx, std::string& outReason) {
    if (!ctx) {
        outReason = "No context";
        return false;
    }

    // T17: Basic blocking checks
    if (!ctx->IsAttached()) {
        outReason = "Not attached";
        return false;
    }

    if (!ctx->IsInGame()) {
        outReason = "Not in game";
        return false;
    }

    if (!ctx->IsGameForeground()) {
        outReason = "Game not foreground";
        return false;
    }

    auto snapshot = ctx->GetSnapshot();
    if (!snapshot) {
        outReason = "No snapshot";
        return false;
    }

    // Block in town (can be overridden by a flag in Phase 6 settings)
    if (snapshot->IsTown) {
        outReason = "In town";
        return false;
    }

    // Block in hideout
    if (snapshot->IsHideout) {
        outReason = "In hideout";
        return false;
    }

    // T18: Player death check (HPPercent is int: 0-100)
    auto vitals = ctx->GetPlayerVitals();
    if (vitals.HPPercent <= 0) {
        outReason = "Player dead";
        return false;
    }

    // T19: Grace period check
    for (const auto& buff : vitals.Buffs) {
        if (buff.Name == "grace_period") {
            outReason = "Grace period";
            return false;
        }
    }

    // All checks passed
    outReason = "Active";
    return true;
}

} // namespace AutoReflex