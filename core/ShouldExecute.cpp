// AutoReflex - ShouldExecute.cpp



#include "ShouldExecute.h"



namespace AutoReflex {



bool DetermineWhetherRulesShouldExecute(

    const PluginSDK::Snapshot& gameSnapshot,

    const Core::EvalTickCache& tickCache,

    std::string& outExecutionGateReason)

{

    if (gameSnapshot.IsTown) {

        outExecutionGateReason = "In town";

        return false;

    }



    if (gameSnapshot.IsHideout) {

        outExecutionGateReason = "In hideout";

        return false;

    }



    if (gameSnapshot.IsPaused) {

        outExecutionGateReason = "Game paused";

        return false;

    }



    if (gameSnapshot.IsSkillTreeVisible) {

        outExecutionGateReason = "Skill tree open";

        return false;

    }



    if (gameSnapshot.Vitals.HPPercent <= 0) {

        outExecutionGateReason = "Player dead";

        return false;

    }



    if (tickCache.PlayerHasGracePeriod()) {

        outExecutionGateReason = "Grace period";

        return false;

    }



    outExecutionGateReason = "Active";

    return true;

}



} // namespace AutoReflex

