// AutoReflex - ShouldExecute

// Gate check: are we in a game state where rules are allowed to fire?



#pragma once



#include "EvalTickCache.h"

#include "../sdk/PluginSDK.h"



#include <string>



namespace AutoReflex {



bool DetermineWhetherRulesShouldExecute(

    const PluginSDK::Snapshot& gameSnapshot,

    const Core::EvalTickCache& tickCache,

    std::string& outExecutionGateReason);



} // namespace AutoReflex

