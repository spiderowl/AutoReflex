// AutoReflex - RuleManager.cpp
// Compiles and evaluates rules using EXPRTK expressions.
//
// Hot path: iterate m_Rules (already sorted by Order), and for each enabled
// rule with a valid compiled expression and an expired cooldown, walk only
// monster entities in the snapshot until one passes. Per-monster work is
// the EXPRTK Evaluate() call, which gates host-bridge reads on flags
// computed at compile time (see ScriptEngine.cpp).

#include "RuleManager.h"
#include "Rule.h"
#include "MonsterCandidateSelection.h"
#include "../scripting/ScriptEngine.h"
#include "../storage/RuleStore.h"
#include "../sdk/PluginContext.h"
#include "../sdk/PluginGameData.h"

#include <chrono>
#include <algorithm>

namespace AutoReflex { namespace Rules {

RuleManager::RuleManager() = default;
RuleManager::~RuleManager() = default;

void RuleManager::LoadAndCompileRulesFromStore(Storage::RuleStore& ruleStore)
{
    ruleStore.LoadAllRulesFromDisk(m_Rules);
    for (auto& rule : m_Rules) {
        CompileRuleExpression(rule);
    }
    SortRulesByEvaluationOrder();
}

void RuleManager::SortRulesByEvaluationOrder()
{
    std::stable_sort(m_Rules.begin(), m_Rules.end(),
        [](const Rule& a, const Rule& b) { return a.Order < b.Order; });
}

void RuleManager::CompileRuleExpression(Rule& rule)
{
    rule.CompileError.clear();
    rule.CompiledExpr.reset();
    rule.Root = (rule.ScriptBody.find("friendlyMonsterCount") != std::string::npos)
        ? RuleRoot::Friendly
        : RuleRoot::Hostile;

    const std::string& expr = rule.ScriptBody;
    if (expr.empty()) {
        rule.CompileError = "Expression is empty";
        return;
    }

    std::string errorMsg;
    if (!ScriptEngine::ValidateUserExpressionString(expr, errorMsg)) {
        rule.CompileError = errorMsg;
        return;
    }

    rule.CompiledExpr = std::make_unique<CompiledExpression>();
    if (!rule.CompiledExpr->CompileExpressionString(expr, rule.CompileError)) {
        rule.CompiledExpr.reset();
    }
}

void RuleManager::EvaluateRulesAgainstSnapshotUntilFirstFire(
    PluginContext* pluginContext,
    const PluginSDK::PluginGameSnapshot* gameSnapshot,
    const std::function<void(const Rule&)>& onRuleFired)
{
    if (!pluginContext || !gameSnapshot) return;

    const auto now = std::chrono::steady_clock::now();

    static constexpr size_t kMaxCandidates = 100;
    static thread_local std::vector<const PluginSDK::RadarEntity*> hostileMonsters;
    static thread_local std::vector<const PluginSDK::RadarEntity*> friendlyMonsters;
    BuildClosestMonsterCandidateListsForSnapshot(
        gameSnapshot,
        kMaxCandidates,
        hostileMonsters,
        friendlyMonsters);

    bool firedAny = false;
    for (Rule& rule : m_Rules) {
        if (firedAny) {
            rule.LastEvalResult = false;
            continue;
        }
        if (!rule.Enabled) continue;
        if (!rule.CompiledExpr || !rule.CompiledExpr->HasCompiledExpression()) continue;

        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - rule.LastFired).count();
        if (elapsedMs < static_cast<long long>(rule.CooldownSec * 1000.0f)) continue;

        const std::vector<const PluginSDK::RadarEntity*>* scanSet = &hostileMonsters;
        if (rule.Root == RuleRoot::Friendly) {
            scanSet = &friendlyMonsters;
        }

        bool fired = false;
        for (const auto* entity : *scanSet) {
            if (rule.CompiledExpr->EvaluateExpressionAgainstEntity(pluginContext, *entity)) {
                fired = true;
                break;
            }
        }

        rule.LastEvalResult = fired;
        if (fired && onRuleFired) {
            onRuleFired(rule);
            rule.LastFired = now;
            rule.EverFired = true;
            firedAny = true;
        }
    }
}

}} // namespace AutoReflex::Rules
