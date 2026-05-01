// AutoReflex - ShouldExecute
// Evaluates whether a rule should fire based on conditions

#pragma once

#include <string>
#include <vector>

namespace AutoReflex {

// Buff condition: check if a specific buff exists (on player or monsters)
struct BuffCondition {
    std::string buffName;           // Name of the buff to look for
    bool checkExists = true;        // true=must exist, false=must NOT exist
    int minCount = 1;               // Minimum number of monsters with this buff
    int maxCount = -1;              // Maximum (-1 = unlimited)
};

// Health condition: check monster health thresholds
struct HealthCondition {
    float minHealthPct = -1.0f;     // Minimum health percentage
    float maxHealthPct = 101.0f;    // Maximum health percentage
};

// ShouldEvaluate: determines if a rule's conditions are met
// Returns true if the rule should fire
class ShouldEvaluate {
public:
    // Set conditions
    ShouldEvaluate& SetBuffConditions(const std::vector<BuffCondition>& conditions);
    ShouldEvaluate& SetHealthCondition(const HealthCondition& condition);
    ShouldEvaluate& SetMinMonsters(int count);
    ShouldEvaluate& SetMaxMonsters(int count);
    ShouldEvaluate& SetScriptCondition(const std::string& script);

    // Evaluate against current game state
    bool Evaluate(void* context, void* conditionState) const;

private:
    std::vector<BuffCondition> m_BuffConditions;
    HealthCondition m_HealthCondition;
    int m_MinMonsters = 1;
    int m_MaxMonsters = -1;
    std::string m_ScriptCondition;  // AngelScript expression
};

} // namespace AutoReflex