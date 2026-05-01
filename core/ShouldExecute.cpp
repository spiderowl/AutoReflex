// AutoReflex - ShouldExecute.cpp
// Implementation of rule evaluation logic

#include "ShouldExecute.h"
#include "../game/ConditionState.h"
#include "sdk/PluginContext.h"

namespace AutoReflex {

ShouldEvaluate& ShouldEvaluate::SetBuffConditions(const std::vector<BuffCondition>& conditions) {
    m_BuffConditions = conditions;
    return *this;
}

ShouldEvaluate& ShouldEvaluate::SetHealthCondition(const HealthCondition& condition) {
    m_HealthCondition = condition;
    return *this;
}

ShouldEvaluate& ShouldEvaluate::SetMinMonsters(int count) {
    m_MinMonsters = count;
    return *this;
}

ShouldEvaluate& ShouldEvaluate::SetMaxMonsters(int count) {
    m_MaxMonsters = count;
    return *this;
}

ShouldEvaluate& ShouldEvaluate::SetScriptCondition(const std::string& script) {
    m_ScriptCondition = script;
    return *this;
}

bool ShouldEvaluate::Evaluate(void* context, void* conditionState) const {
    auto* cs = static_cast<Game::ConditionState*>(conditionState);
    if (!cs) return false;

    // Check monster count
    int totalMonsters = static_cast<int>(cs->GetMonsterCount());
    if (totalMonsters < m_MinMonsters) return false;
    if (m_MaxMonsters >= 0 && totalMonsters > m_MaxMonsters) return false;

    // Check buff conditions
    for (const auto& bc : m_BuffConditions) {
        int count = cs->CountMonstersWithBuff(bc.buffName);
        if (bc.checkExists && count < bc.minCount) return false;
        if (bc.maxCount >= 0 && count > bc.maxCount) return false;
        if (!bc.checkExists && count > 0) return false;
    }

    // Check health condition (applied to any matching monster)
    if (m_HealthCondition.minHealthPct >= 0 || m_HealthCondition.maxHealthPct < 101.0f) {
        bool anyMatch = false;
        for (const auto& monster : cs->GetMonsters()) {
            float hpPct = monster.HealthPercent;
            if (hpPct >= m_HealthCondition.minHealthPct && hpPct <= m_HealthCondition.maxHealthPct) {
                anyMatch = true;
                break;
            }
        }
        if (!anyMatch) return false;
    }

    // Script condition evaluated separately by ScriptEngine
    // (handled in RuleManager::EvaluateAll)

    return true;
}

} // namespace AutoReflex