// AutoReflex - Rule
// Defines a single automation rule with AngelScript condition

#pragma once
#include <string>
#include <chrono>
#include <cstdint>

class asIScriptModule;

namespace AutoReflex {
namespace Rules {

struct Rule {
    // User-editable fields (saved to disk)
    std::string  Name;
    bool         Enabled      = false;
    uint16_t     Key          = 0;
    float        CooldownSec  = 1.0f;     // Seconds between uses
    float        WaitAfterPressMs = 50.0f; // Milliseconds to sleep after keypress (animation time)
    int          Order        = 0;
    std::string  ScriptBody;   // body only, no boilerplate

    // Runtime fields (not saved)
    asIScriptModule*                          Module       = nullptr;
    std::string                               CompileError;
    bool                                      LastEvalResult = false;
    std::chrono::steady_clock::time_point     LastFired;
    bool                                      EverFired    = false;
};

} // namespace Rules
} // namespace AutoReflex