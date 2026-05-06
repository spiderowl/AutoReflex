// AutoReflex - Rule
// Defines a single automation rule with EXPRTK expression condition

#pragma once
#include <string>
#include <chrono>
#include <cstdint>
#include <memory>

// Include full CompiledExpression definition (needed by unique_ptr for reset/delete)
#include "scripting/ScriptEngine.h"

namespace AutoReflex {
namespace Rules {

enum class RuleRoot : uint8_t {
    Hostile = 0,   // monsterCount
    Friendly = 1,  // friendlyMonsterCount
};

struct Rule {
    // User-editable fields (saved to disk)
    std::string  Name;
    bool         Enabled      = false;
    uint16_t     Key          = 0;
    float        CooldownSec  = 1.0f;     // Seconds between uses
    float        WaitAfterPressMs = 50.0f; // Milliseconds to sleep after keypress (animation time)
    int          Order        = 0;
    std::string  ScriptBody;   // EXPRTK boolean expression, e.g.:
                               //   e_IsValid and (e_CurrentHP / e_MaxHP) > 0.5 and !e_IsSleeping

    // Runtime fields (not saved)
    std::unique_ptr<CompiledExpression>   CompiledExpr;
    std::string                           CompileError;
    bool                                  LastEvalResult = false;
    std::chrono::steady_clock::time_point LastFired;
    bool                                  EverFired    = false;
    RuleRoot                              Root = RuleRoot::Hostile;
};

} // namespace Rules
} // namespace AutoReflex