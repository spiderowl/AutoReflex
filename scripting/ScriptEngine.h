// AutoReflex - ScriptEngine.h
// EXPRTK-based expression evaluation engine (header-only, no external dependency)
// Replaces AngelScript with lightweight mathematical expression evaluation
//
// Each rule's condition is a boolean expression over entity fields:
//   e.IsValid and e.CurrentHP > 100 and !e.IsSleeping
//
// Variable mapping (entity fields exposed as 'e.<field>'):
//   e.Id, e.IsValid, e.Rarity, e.GridPositionX, e.GridPositionY
//   e.WorldX, e.WorldY, e.WorldZ
//   e.CurrentHP, e.MaxHP, e.CurrentES, e.MaxES, e.IsSleeping

#pragma once

#include <string>
#include <memory>
#include "sdk/PluginGameData.h"

// EXPRTK is header-only - include the single header
#include "exprtk.hpp"

// Forward declarations replaced by direct includes above

// ============================================================================
// CompiledExpression - holds a pre-compiled EXPRTK expression for a single rule
// ============================================================================

class CompiledExpression {
public:
    CompiledExpression();
    ~CompiledExpression();

    // Compile an expression string; returns true on success
    bool Compile(const std::string& exprString, std::string& errorMsg);

    // Evaluate the expression against a RadarEntity; returns the boolean result
    // cursorX/cursorY are grid-coordinate position of the mouse cursor
    bool Evaluate(const PluginSDK::RadarEntity& entity,
                  double cursorX, double cursorY) const;

    // Check if this expression is valid
    bool IsValid() const { return expression_ != nullptr; }

    // Get the source expression string
    const std::string& GetExpressionString() const { return exprString_; }

private:
    std::string exprString_;

    // EXPRTK uses smart pointers internally for expression nodes
    std::unique_ptr<exprtk::expression<double>> expression_;
    std::unique_ptr<exprtk::symbol_table<double>> symbolTable_;
    std::unique_ptr<exprtk::parser<double>> parser_;

    // Entity field variables (bound by reference during evaluation)
    mutable double e_Id;
    mutable double e_IsValid;
    mutable double e_Rarity;
    mutable double e_GridPositionX;
    mutable double e_GridPositionY;
    mutable double e_GridPositionZ;
    mutable double e_WorldX;
    mutable double e_WorldY;
    mutable double e_WorldZ;
    mutable double e_CurrentHP;
    mutable double e_MaxHP;
    mutable double e_CurrentES;
    mutable double e_MaxES;
    mutable double e_IsSleeping;

    // Context variables (set per-evaluation, e.g. cursor position)
    mutable double curX;
    mutable double curY;
};

// ============================================================================
// ScriptEngine - global singleton for EXPRTK initialization
// ============================================================================

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    // Initialize ( lightweight - EXPRTK is header-only so this is mostly no-op )
    bool Initialize();

    // Check if the engine is ready
    bool IsInitialized() const { return initialized_; }

    // Get last error message
    const std::string& GetLastError() const { return lastError; }

    // Check if an expression string is valid syntax (quick compile test)
    static bool ValidateExpression(const std::string& expr, std::string& errorMsg);

private:
    bool initialized_ = false;
    std::string lastError;
};

// Global singleton access
ScriptEngine& GetScriptEngine();