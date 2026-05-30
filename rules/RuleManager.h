// AutoReflex - RuleManager.h

#pragma once

#include "Rule.h"

#include "../core/AnimationLock.h"
#include "../core/EvalTickCache.h"

#include <vector>
#include <functional>
#include <string>
#include <memory>

#include "../sdk/PluginSDK.h"

namespace AutoReflex { namespace Storage { class RuleStore; }}

namespace AutoReflex {
namespace Rules {

class RuleManager {
public:
    RuleManager();
    ~RuleManager();

    void LoadAndCompileRulesFromStore(Storage::RuleStore& ruleStore);

    void CompileRuleExpression(Rule& rule);

    void EvaluateRulesAgainstSnapshotUntilFirstFire(
        const PluginSDK::Context& pluginContext,
        const PluginSDK::Snapshot& gameSnapshot,
        Core::EvalTickCache& tickCache,
        Core::AnimationLock& animationLock,
        const std::function<void(const Rule&)>& onRuleFired);

    void SortRulesByEvaluationOrder();

    std::vector<Rule>& GetRules() { return m_Rules; }
    const std::vector<Rule>& GetRules() const { return m_Rules; }

private:
    std::vector<Rule> m_Rules;
};

} // namespace Rules
} // namespace AutoReflex
