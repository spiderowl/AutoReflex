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
#include "../scripting/ScriptEngine.h"
#include "../storage/RuleStore.h"
#include "../sdk/PluginContext.h"
#include "../sdk/PluginGameData.h"

#include <chrono>
#include <algorithm>

namespace AutoReflex { namespace Rules {

RuleManager::RuleManager() = default;
RuleManager::~RuleManager() = default;

void RuleManager::LoadRules(Storage::RuleStore& store)
{
    store.LoadAll(m_Rules);
    for (auto& rule : m_Rules) {
        CompileRule(rule);
    }
    SortByOrder();
}

void RuleManager::SortByOrder()
{
    std::stable_sort(m_Rules.begin(), m_Rules.end(),
        [](const Rule& a, const Rule& b) { return a.Order < b.Order; });
}

void RuleManager::CompileRule(Rule& rule)
{
    rule.CompileError.clear();
    rule.CompiledExpr.reset();

    const std::string& expr = rule.ScriptBody;
    if (expr.empty()) {
        rule.CompileError = "Expression is empty";
        return;
    }

    std::string errorMsg;
    if (!ScriptEngine::ValidateExpression(expr, errorMsg)) {
        rule.CompileError = errorMsg;
        return;
    }

    rule.CompiledExpr = std::make_unique<CompiledExpression>();
    if (!rule.CompiledExpr->Compile(expr, rule.CompileError)) {
        rule.CompiledExpr.reset();
    }
}

void RuleManager::EvaluateAll(
    PluginContext* ctx,
    const PluginSDK::PluginGameSnapshot* snapshot,
    const std::function<void(const Rule&)>& onFire)
{
    if (!ctx || !snapshot) return;

    const auto now = std::chrono::steady_clock::now();
    const auto& entities = snapshot->Entities;

    for (Rule& rule : m_Rules) {
        if (!rule.Enabled) continue;
        if (!rule.CompiledExpr || !rule.CompiledExpr->IsValid()) continue;

        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - rule.LastFired).count();
        if (elapsedMs < static_cast<long long>(rule.CooldownSec * 1000.0f)) continue;

        bool fired = false;
        for (const auto& entity : entities) {
            if (entity.entityType != PluginSDK::EntityTypes::Monster) continue;
            if (!entity.IsValid) continue;
            if (rule.CompiledExpr->Evaluate(ctx, entity)) {
                fired = true;
                break;
            }
        }

        rule.LastEvalResult = fired;
        if (fired && onFire) {
            onFire(rule);
            rule.LastFired = now;
            rule.EverFired = true;
        }
    }
}

}} // namespace AutoReflex::Rules
