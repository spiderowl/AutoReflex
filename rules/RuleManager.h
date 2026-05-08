// AutoReflex - RuleManager
// Owns the rule list, compiles EXPRTK expressions, evaluates conditions
// against monsters in a snapshot. Rules are stored sorted by Order so the
// hot path is a flat walk with no per-frame allocations.

#pragma once

#include "Rule.h"

#include <vector>
#include <functional>
#include <string>
#include <memory>

struct PluginContext;
namespace PluginSDK { struct PluginGameSnapshot; }
namespace AutoReflex { namespace Storage { class RuleStore; }}

namespace AutoReflex {
namespace Rules {

class RuleManager {
public:
    RuleManager();
    ~RuleManager();

    /**
     * Loads all persisted rules from a store, compiles them, and sorts them by evaluation order.
     *
     * @param ruleStore Store used to load persisted rule definitions.
     * @returns None.
     */
    void LoadAndCompileRulesFromStore(Storage::RuleStore& ruleStore);

    /**
     * Compiles a rule's expression and updates its runtime fields.
     *
     * @param rule Rule to compile in-place.
     * @returns None.
     */
    void CompileRuleExpression(Rule& rule);

    /**
     * Evaluates enabled rules against snapshot monsters, in increasing `Order`, stopping after the
     * first rule fires for the tick.
     *
     * @param pluginContext Host bridge used for evaluation (cursor projection, buffs, etc.).
     * @param gameSnapshot Snapshot providing the entity list to scan.
     * @param onRuleFired Callback invoked for the first rule that fires this tick.
     * @returns None.
     */
    void EvaluateRulesAgainstSnapshotUntilFirstFire(
        PluginContext* pluginContext,
        const PluginSDK::PluginGameSnapshot* gameSnapshot,
        const std::function<void(const Rule&)>& onRuleFired);

    /**
     * Sorts rules by the persisted `Order` field so evaluation is a flat scan.
     *
     * @returns None.
     */
    void SortRulesByEvaluationOrder();

    std::vector<Rule>& GetRules() { return m_Rules; }
    const std::vector<Rule>& GetRules() const { return m_Rules; }

private:
    std::vector<Rule> m_Rules;
};

} // namespace Rules
} // namespace AutoReflex
