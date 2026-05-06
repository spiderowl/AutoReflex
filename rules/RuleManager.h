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

    // Load all rules from disk and compile.
    void LoadRules(Storage::RuleStore& store);

    // Compile a single rule's expression.
    void CompileRule(Rule& rule);

    // Evaluate every enabled rule against monsters in `snapshot`.
    // For each rule whose condition returns true on at least one monster,
    // `onFire` is invoked once. The list is iterated in `Order`.
    void EvaluateAll(
        PluginContext* ctx,
        const PluginSDK::PluginGameSnapshot* snapshot,
        const std::function<void(const Rule&)>& onFire);

    // Re-sort the rule list by Order. Call after LoadRules and after any
    // Up/Down move so the hot path can iterate `m_Rules` directly.
    void SortByOrder();

    std::vector<Rule>& GetRules() { return m_Rules; }
    const std::vector<Rule>& GetRules() const { return m_Rules; }

private:
    std::vector<Rule> m_Rules;
};

} // namespace Rules
} // namespace AutoReflex
