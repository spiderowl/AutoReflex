// AutoReflex - rules/RuleManager.cpp (Phase 7+)
#include "RuleManager.h"

namespace AutoReflex { namespace Rules {
    RuleManager::RuleManager(const std::string& /*dataDir*/) {}
    void RuleManager::LoadRules(Storage::RuleStore& /*store*/) {}
    void RuleManager::EvaluateAll(PluginContext* /*ctx*/, Game::ConditionState& /*cs*/, std::function<void(const Rule&)> /*onFire*/) {}
}}