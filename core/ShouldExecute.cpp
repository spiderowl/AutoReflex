// AutoReflex - ShouldExecute.cpp

#include "ShouldExecute.h"
#include "sdk/PluginContext.h"
#include "sdk/PluginGameData.h"

namespace AutoReflex {

bool ShouldExecute(PluginContext* ctx,
                   const PluginSDK::PluginGameSnapshot* snapshot,
                   std::string& outReason)
{
    if (!ctx) {
        outReason = "No context";
        return false;
    }

    if (!ctx->IsAttached || !ctx->IsAttached()) {
        outReason = "Not attached";
        return false;
    }

    if (!ctx->IsInGame || !ctx->IsInGame()) {
        outReason = "Not in game";
        return false;
    }

    if (!ctx->IsGameForeground || !ctx->IsGameForeground()) {
        outReason = "Game not foreground";
        return false;
    }

    if (!snapshot) {
        outReason = "No snapshot";
        return false;
    }

    if (snapshot->IsTown) {
        outReason = "In town";
        return false;
    }

    if (snapshot->IsHideout) {
        outReason = "In hideout";
        return false;
    }

    // Use the vitals already on the snapshot — same data the host would hand
    // back via GetPlayerVitals(), but without the extra bridge call.
    const auto& vitals = snapshot->Vitals;
    if (vitals.HPPercent <= 0) {
        outReason = "Player dead";
        return false;
    }

    for (const auto& buff : vitals.Buffs) {
        if (buff.Name == "grace_period") {
            outReason = "Grace period";
            return false;
        }
    }

    outReason = "Active";
    return true;
}

} // namespace AutoReflex
