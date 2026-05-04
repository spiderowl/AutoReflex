// AutoReflex - ScriptEngine.cpp
// EXPRTK-based expression evaluation engine
// Header-only library — no external linking required

#include "ScriptEngine.h"
#include "../core/AgentDebugLog.h"

#include "exprtk.hpp"

#include <sstream>
#include <cassert>
#include <iostream>

// ============================================================================
// CompiledExpression implementation
// ============================================================================

typedef exprtk::symbol_table<double> symbol_table_t;
typedef exprtk::expression<double>   expression_t;
typedef exprtk::parser<double>       parser_t;

CompiledExpression::CompiledExpression()
    : expression_(std::make_unique<expression_t>())
    , symbolTable_(std::make_unique<symbol_table_t>())
    , parser_(std::make_unique<parser_t>())
{
    // Register entity field variables with the symbol table
    symbolTable_->add_variable("e_Id", e_Id);
    symbolTable_->add_variable("e_IsValid", e_IsValid);
    symbolTable_->add_variable("e_Rarity", e_Rarity);
    symbolTable_->add_variable("e_GridPositionX", e_GridPositionX);
    symbolTable_->add_variable("e_GridPositionY", e_GridPositionY);
    symbolTable_->add_variable("e_GridPositionZ", e_GridPositionZ);
    symbolTable_->add_variable("e_WorldX", e_WorldX);
    symbolTable_->add_variable("e_WorldY", e_WorldY);
    symbolTable_->add_variable("e_WorldZ", e_WorldZ);
    symbolTable_->add_variable("e_CurrentHP", e_CurrentHP);
    symbolTable_->add_variable("e_MaxHP", e_MaxHP);
    symbolTable_->add_variable("e_CurrentES", e_CurrentES);
    symbolTable_->add_variable("e_MaxES", e_MaxES);
    symbolTable_->add_variable("e_IsSleeping", e_IsSleeping);

    // Register context variables (cursor position in grid coordinates)
    curX = 0.0;
    curY = 0.0;
    symbolTable_->add_variable("curX", curX);
    symbolTable_->add_variable("curY", curY);

    // Register mathematical constants
    symbolTable_->add_constants();

    // Register the symbol table with the expression
    expression_->register_symbol_table(*symbolTable_);
}

CompiledExpression::~CompiledExpression() = default;

bool CompiledExpression::Compile(const std::string& exprString, std::string& errorMsg) {
    exprString_ = exprString;

    // Attempt to parse the expression
    bool success = parser_->compile(exprString, *expression_);

    if (!success) {
        errorMsg = "Expression compilation failed: '" + exprString + "'";
        // #region agent log
        ArAgentNdjsonLog("H-EXPR", "CompiledExpression::Compile", "Expression compilation failed: " + exprString, "{}");
        // #endregion
        return false;
    }

    errorMsg.clear();
    // #region agent log
    {
        std::ostringstream d;
        d << "{\"exprLen\":" << exprString.size() << "}";
        ArAgentNdjsonLog("H-EXPR", "CompiledExpression::Compile", "Expression compiled successfully", d.str());
    }
    // #endregion
    return true;
}

bool CompiledExpression::Evaluate(const PluginSDK::RadarEntity& entity,
                                  double cursorX, double cursorY) const
{
    if (!expression_) return false;

    // Bind entity fields to variables
    e_Id              = static_cast<double>(entity.Id);
    e_IsValid         = entity.IsValid ? 1.0 : 0.0;
    e_Rarity          = static_cast<double>(entity.Rarity);
    e_GridPositionX   = static_cast<double>(entity.GridPositionX);
    e_GridPositionY   = static_cast<double>(entity.GridPositionY);
    e_GridPositionZ   = 0.0; // Not directly available, set to 0
    e_WorldX          = static_cast<double>(entity.WorldX);
    e_WorldY          = static_cast<double>(entity.WorldY);
    e_WorldZ          = static_cast<double>(entity.WorldZ);
    e_CurrentHP       = static_cast<double>(entity.CurrentHP);
    e_MaxHP           = static_cast<double>(entity.MaxHP);
    e_CurrentES       = static_cast<double>(entity.CurrentES);
    e_MaxES           = static_cast<double>(entity.MaxES);
    e_IsSleeping      = entity.IsSleeping ? 1.0 : 0.0;

    // Bind context variables
    curX = cursorX;
    curY = cursorY;

    // Evaluate — result is in expression_->value()
    double result = expression_->value();

    // Treat nonzero as true (boolean semantics)
    return result != 0.0;
}

// ============================================================================
// ScriptEngine implementation
// ============================================================================

ScriptEngine::ScriptEngine() {}

ScriptEngine::~ScriptEngine() {}

bool ScriptEngine::Initialize() {
    lastError.clear();
    initialized_ = true;

    // #region agent log
    ArAgentNdjsonLog("H-INIT", "ScriptEngine::Initialize", "EXPRTK engine initialized (header-only)", "{}");
    // #endregion

    return true;
}

bool ScriptEngine::ValidateExpression(const std::string& expr, std::string& errorMsg) {
    // Quick compile-test without keeping the compiled result
    symbol_table_t symbolTable;
    expression_t expression;
    parser_t parser;

    // Register the same variables
    double dummy = 0.0;
    symbolTable.add_variable("e_Id", dummy);
    symbolTable.add_variable("e_IsValid", dummy);
    symbolTable.add_variable("e_Rarity", dummy);
    symbolTable.add_variable("e_GridPositionX", dummy);
    symbolTable.add_variable("e_GridPositionY", dummy);
    symbolTable.add_variable("e_GridPositionZ", dummy);
    symbolTable.add_variable("e_WorldX", dummy);
    symbolTable.add_variable("e_WorldY", dummy);
    symbolTable.add_variable("e_WorldZ", dummy);
    symbolTable.add_variable("e_CurrentHP", dummy);
    symbolTable.add_variable("e_MaxHP", dummy);
    symbolTable.add_variable("e_CurrentES", dummy);
    symbolTable.add_variable("e_MaxES", dummy);
    symbolTable.add_variable("e_IsSleeping", dummy);
    symbolTable.add_variable("curX", dummy);
    symbolTable.add_variable("curY", dummy);
    symbolTable.add_constants();

    expression.register_symbol_table(symbolTable);

    bool success = parser.compile(expr, expression);
    if (!success) {
        errorMsg = "Expression syntax error: '" + expr + "'";
    }
    return success;
}

// ============================================================================
// Global singleton
// ============================================================================

ScriptEngine& GetScriptEngine() {
    static ScriptEngine instance;
    return instance;
}