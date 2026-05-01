// AutoReflex - Rule
// Defines a single automation rule

#pragma once
#include <string>
#include <vector>
#include "../core/ShouldExecute.h"

namespace AutoReflex {
namespace Rules {

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