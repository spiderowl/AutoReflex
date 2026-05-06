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
    rule.Root = (rule.ScriptBody.find("friendlyMonsterCount") != std::string::npos)
        ? RuleRoot::Friendly
        : RuleRoot::Hostile;

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
    const float px = snapshot->Player.GridPositionX;
    const float py = snapshot->Player.GridPositionY;

    // One-pass base filter shared across all rules.
    // This avoids re-checking the same cheap predicates for every rule when many rules are enabled.
    static constexpr size_t kMaxCandidates = 100;
    // IMPORTANT: reuse vectors across ticks to avoid heap churn at 30Hz.
    static thread_local std::vector<std::pair<float, const PluginSDK::RadarEntity*>> hostileScored;
    static thread_local std::vector<std::pair<float, const PluginSDK::RadarEntity*>> friendlyScored;
    static thread_local std::vector<const PluginSDK::RadarEntity*> hostileMonsters;
    static thread_local std::vector<const PluginSDK::RadarEntity*> friendlyMonsters;
    static thread_local bool reserved = false;
    if (!reserved) {
        hostileScored.reserve(512);
        friendlyScored.reserve(256);
        hostileMonsters.reserve(kMaxCandidates);
        friendlyMonsters.reserve(kMaxCandidates);
        reserved = true;
    }
    hostileScored.clear();
    friendlyScored.clear();

    for (const auto& entity : entities) {
        if (entity.entityType != PluginSDK::EntityTypes::Monster) continue;
        if (!entity.IsValid) continue;
        if (entity.CurrentHP <= 0) continue;    // alive only
        if (entity.IsSleeping) continue;        // awake only
        // Inner/Outer only (far/none excluded) — cheap and shrinks the scan set.
        if (!(entity.Zone == PluginSDK::NearbyZone::InnerCircle
           || entity.Zone == PluginSDK::NearbyZone::OuterCircle)) continue;
        const float dx = entity.GridPositionX - px;
        const float dy = entity.GridPositionY - py;
        const float d2 = dx * dx + dy * dy;
        if (entity.Reaction == 0) hostileScored.emplace_back(d2, &entity);
        else if (entity.Reaction == 2) friendlyScored.emplace_back(d2, &entity);
    }

    auto takeClosest = [&](std::vector<std::pair<float, const PluginSDK::RadarEntity*>>& scored,
                           std::vector<const PluginSDK::RadarEntity*>& out) {
        out.clear();
        if (scored.empty()) return;
        const size_t take = std::min(kMaxCandidates, scored.size());
        // nth_element requires nth in [begin, end); if we take all, skip.
        if (take < scored.size()) {
            std::nth_element(scored.begin(), scored.begin() + take, scored.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
        }
        out.reserve(take);
        for (size_t i = 0; i < take; ++i) out.push_back(scored[i].second);
    };

    takeClosest(hostileScored, hostileMonsters);
    takeClosest(friendlyScored, friendlyMonsters);

    bool firedAny = false;
    for (Rule& rule : m_Rules) {
        if (firedAny) {
            rule.LastEvalResult = false;
            continue;
        }
        if (!rule.Enabled) continue;
        if (!rule.CompiledExpr || !rule.CompiledExpr->IsValid()) continue;

        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - rule.LastFired).count();
        if (elapsedMs < static_cast<long long>(rule.CooldownSec * 1000.0f)) continue;

        const std::vector<const PluginSDK::RadarEntity*>* scanSet = &hostileMonsters;
        if (rule.Root == RuleRoot::Friendly) {
            scanSet = &friendlyMonsters;
        }

        bool fired = false;
        for (const auto* entity : *scanSet) {
            if (rule.CompiledExpr->Evaluate(ctx, *entity)) {
                fired = true;
                break;
            }
        }

        rule.LastEvalResult = fired;
        if (fired && onFire) {
            onFire(rule);
            rule.LastFired = now;
            rule.EverFired = true;
            firedAny = true;
        }
    }
}

}} // namespace AutoReflex::Rules
