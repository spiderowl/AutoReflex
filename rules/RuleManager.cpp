// AutoReflex - RuleManager.cpp

#include "RuleManager.h"
#include "Rule.h"
#include "MonsterCandidateSelection.h"
#include "../scripting/ScriptEngine.h"
#include "../storage/RuleStore.h"

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
    rule.UsesMonsterScan =
        rule.ScriptBody.find("monsterCount") != std::string::npos
        || rule.ScriptBody.find("friendlyMonsterCount") != std::string::npos;

    const std::string& expr = rule.ScriptBody;
    if (expr.empty()) {
        rule.CompileError = "Expression is empty";
        return;
    }

    rule.CompiledExpr = std::make_unique<CompiledExpression>();
    if (!rule.CompiledExpr->CompileExpressionString(expr, rule.CompileError)) {
        rule.CompiledExpr.reset();
        return;
    }
}

void RuleManager::EvaluateRulesAgainstSnapshotUntilFirstFire(
    const PluginSDK::Context& pluginContext,
    const PluginSDK::Snapshot& gameSnapshot,
    Core::EvalTickCache& tickCache,
    Core::AnimationLock& animationLock,
    const std::function<void(const Rule&)>& onRuleFired)
{
    const Core::EvalTickCacheScope tickCacheScope(tickCache);

    const auto now = std::chrono::steady_clock::now();

    bool buildHostileCandidates = false;
    bool buildFriendlyCandidates = false;
    for (const Rule& rule : m_Rules) {
        if (!rule.Enabled || !rule.CompiledExpr || !rule.CompiledExpr->HasCompiledExpression()) {
            continue;
        }
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - rule.LastFired).count();
        if (elapsedMs < static_cast<long long>(rule.CooldownSec * 1000.0f)) continue;

        if (rule.UsesMonsterScan) {
            if (rule.Root == RuleRoot::Friendly) buildFriendlyCandidates = true;
            else buildHostileCandidates = true;
        }
    }

    static constexpr size_t kMaxCandidates = 100;
    static thread_local std::vector<const PluginSDK::Entity*> hostileMonsters;
    static thread_local std::vector<const PluginSDK::Entity*> friendlyMonsters;
    BuildClosestMonsterCandidateListsForSnapshot(
        gameSnapshot,
        kMaxCandidates,
        buildHostileCandidates,
        buildFriendlyCandidates,
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

        const std::vector<const PluginSDK::Entity*>* scanSet = &hostileMonsters;
        if (rule.Root == RuleRoot::Friendly) {
            scanSet = &friendlyMonsters;
        }

        bool fired = false;
        if (!rule.UsesMonsterScan) {
            fired = rule.CompiledExpr->EvaluatePlayerCondition(pluginContext);
        } else {
            if (scanSet->empty()) continue;

            for (const auto* entity : *scanSet) {
                if (rule.CompiledExpr->EvaluateExpressionAgainstEntity(pluginContext, *entity)) {
                    fired = true;
                    break;
                }
            }
        }

        rule.LastEvalResult = fired;
        if (fired && onRuleFired) {
            onRuleFired(rule);
            rule.LastFired = now;
            rule.EverFired = true;
            animationLock.Engage(rule.WaitAfterPressMs);
            firedAny = true;
        }
    }
}

}} // namespace AutoReflex::Rules
