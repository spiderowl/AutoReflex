// AutoReflex - RuleManager.cpp
// Compiles and evaluates rules using EXPRTK expressions

#include "RuleManager.h"
#include "Rule.h"
#include "../core/AgentDebugLog.h"
#include "../game/ConditionState.h"
#include "../scripting/ScriptEngine.h"
#include "../storage/RuleStore.h"
#include "../sdk/PluginContext.h"

#include <chrono>
#include <sstream>
#include <algorithm>
#include <numeric>

namespace AutoReflex { namespace Rules {

RuleManager::RuleManager()
{
}

RuleManager::~RuleManager()
{
}

void RuleManager::LoadRules(Storage::RuleStore& store)
{
    // Load all rules from disk into m_Rules
    store.LoadAll(m_Rules);

    // Compile each loaded rule
    for (auto& rule : m_Rules) {
        CompileRule(rule);
    }
}

void RuleManager::CompileRule(Rule& rule)
{
    rule.CompileError.clear();
    rule.CompiledExpr.reset();

    // ScriptBody should be an EXPRTK boolean expression
    std::string expr = rule.ScriptBody;

    if (expr.empty()) {
        rule.CompileError = "Expression is empty";
        // #region agent log
        ArAgentNdjsonLog("H-BUILD", "RuleManager::CompileRule", "Expression is empty for rule: " + rule.Name, "{}");
        // #endregion
        return;
    }

    // Validate the expression first
    std::string errorMsg;
    if (!ScriptEngine::ValidateExpression(expr, errorMsg)) {
        rule.CompileError = errorMsg;
        // #region agent log
        ArAgentNdjsonLog("H-BUILD", "RuleManager::CompileRule.ValidateFailed",
            std::string("rule=") + rule.Name + " err=" + errorMsg,
            "{}");
        // #endregion
        return;
    }

    // Create and compile the expression
    rule.CompiledExpr = std::make_unique<CompiledExpression>();
    if (!rule.CompiledExpr->Compile(expr, rule.CompileError)) {
        rule.CompiledExpr.reset();
        // #region agent log
        ArAgentNdjsonLog("H-BUILD", "RuleManager::CompileRule.CompileFailed",
            std::string("rule=") + rule.Name + " err=" + rule.CompileError,
            "{}");
        // #endregion
        return;
    }

    // #region agent log
    {
        std::ostringstream d;
        d << "{\"ruleName\":\"" << ArJsonEsc(rule.Name)
          << "\",\"exprLen\":" << expr.size() << "}";
        ArAgentNdjsonLog("H-BUILD", "RuleManager::CompileRule.BuildOk", "Rule compiled successfully", d.str());
    }
    // #endregion
}

void RuleManager::EvaluateAll(
    PluginContext* ctx,
    Game::ConditionState& conditionState,
    std::function<void(const Rule&)> onFire)
{
    if (!ctx) return;
    auto now = std::chrono::steady_clock::now();

    // Snapshot is consistent for this evaluation pass; fetch once per frame.
    auto snapshot = ctx->GetSnapshot();
    if (!snapshot) return;

    // Sort by order (lower = higher priority)
    std::vector<size_t> indices(m_Rules.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return m_Rules[a].Order < m_Rules[b].Order;
    });

    for (size_t idx : indices) {
        Rule& rule = m_Rules[idx];

        // Skip disabled rules
        if (!rule.Enabled) continue;

        // Skip if no compiled expression
        if (!rule.CompiledExpr || !rule.CompiledExpr->IsValid()) continue;

        // Check cooldown
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - rule.LastFired).count();
        if (elapsed < rule.CooldownSec * 1000.0f) continue;

        // Evaluate EXPRTK expression against each entity
        bool conditionMet = false;

        // Get cursor position in grid coordinates
        auto curPos = conditionState.CursorPos();
        double curX = static_cast<double>(curPos.x);
        double curY = static_cast<double>(curPos.y);

        // Get Entities from PluginContext snapshot
        for (const auto& entity : snapshot->Entities) {
            if (!entity.IsValid) continue;

            // Evaluate the EXPRTK expression against this entity
            if (rule.CompiledExpr->Evaluate(ctx, entity, curX, curY)) {
                conditionMet = true;
                break;
            }
        }

        rule.LastEvalResult = conditionMet;

        // If condition is true, fire the rule
        if (conditionMet && onFire) {
            onFire(rule);
            rule.LastFired = now;
            rule.EverFired = true;
        }
    }
}

} } // namespace AutoReflex::Rules