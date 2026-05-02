// AutoReflex - Rule
// Defines a single automation rule

#pragma once
#include <string>
#include <vector>

namespace AutoReflex {
namespace Rules {

// Forward declaration (defined in RuleManager.cpp)
struct BuffCondition {
    std::string BuffNamePattern;  // e.g. "free_movement" or contains:free
    bool MatchContains = true;    // true=contains match, false=exact match
    bool RequirePresent = true;   // true=buff must be present, false=buff must be absent
};

struct Rule {
    std::string Name;
    bool Enabled = true;
    std::vector<BuffCondition> BuffConditions;
    float MinHealthPct = -1.0f;
    float MaxHealthPct = 101.0f;
    int MinMonsters = 1;
    int MaxMonsters = -1;
    std::string SimKey;       // Virtual key code string, e.g. "0x70" = P
    std::string ConditionScript; // AngelScript condition
};

} // namespace Rules
} // namespace AutoReflex