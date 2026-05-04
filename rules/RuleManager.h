// AutoReflex - RuleManager
// Manages collection of rules, compiles EXPRTK expressions, evaluates conditions

#pragma once
#include "Rule.h"
#include <vector>
#include <functional>
#include <string>

struct PluginContext;
namespace AutoReflex { namespace Game { class ConditionState; }}
namespace AutoReflex { namespace Storage { class RuleStore; }}

namespace AutoReflex {
namespace Rules {

class RuleManager {
public:
    RuleManager();
    ~RuleManager();

    // Load all rules from disk
    void LoadRules(Storage::RuleStore& store);

    // Compile a single rule's EXPRTK expression
    void CompileRule(Rule& rule);

    // Evaluate all enabled rules; call onFire for each that fires
    void EvaluateAll(
        PluginContext* ctx,
        Game::ConditionState& conditionState,
        std::function<void(const Rule&)> onFire);

    std::vector<Rule>& GetRules() { return m_Rules; }
    const std::vector<Rule>& GetRules() const { return m_Rules; }

    // Selected rule index for UI
    int SelectedRuleIndex = -1;

private:
    std::vector<Rule> m_Rules;
};

} // namespace Rules
} // namespace AutoReflex