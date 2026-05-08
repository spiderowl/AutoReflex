// AutoReflex - ScriptEngine.h
// EXPRTK-based expression evaluation engine (header-only, no external dependency).
//
// Each rule's condition is a boolean expression over a single entity, e.g.:
//   e_IsValid and e_CurrentHP > 100 and !e_IsSleeping
//
// Per-entity variables exposed to the expression:
//   e_Id, e_IsValid, e_Rarity, e_EntityState
//   e_GridPositionX, e_GridPositionY
//   e_WorldX, e_WorldY, e_WorldZ
//   e_CurrentHP, e_MaxHP, e_CurrentES, e_MaxES, e_IsSleeping
//   e_CursorDistPx, e_Reaction
//
// Host-bridge calls (WorldToScreen, ReadBuffsComponent) are gated on per-expression
// "needs" flags computed at Compile time so that expressions that don't reference,
// e.g., e_CursorDistPx never pay for it.

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include "sdk/PluginGameData.h"

#include "exprtk.hpp"

struct PluginContext;

class CompiledExpression {
public:
    CompiledExpression();
    ~CompiledExpression();

    /**
     * Compiles a user-authored expression string into an EXPRTK expression.
     *
     * @param rawExpressionString User-authored script body (may include DSL conveniences).
     * @param outErrorMessage On failure, receives a user-facing compilation error message.
     * @returns True when compilation succeeded; otherwise false.
     */
    bool CompileExpressionString(const std::string& rawExpressionString, std::string& outErrorMessage);

    /**
     * Evaluates the compiled expression against a single entity.
     *
     * @param pluginContext Host bridge used for optional cursor/buff/path queries.
     * @param radarEntity Entity whose fields are bound to `e_*` variables.
     * @returns True when the expression evaluates non-zero; otherwise false.
     */
    bool EvaluateExpressionAgainstEntity(
        PluginContext* pluginContext,
        const PluginSDK::RadarEntity& radarEntity) const;

    /** Returns whether an expression was successfully compiled and is ready to evaluate. */
    bool HasCompiledExpression() const { return expression_ != nullptr; }

    // Internal helpers used by EXPRTK thunks.
    bool   HasBuffIdx(int idx) const;
    double HasBuffValueIdx(int idx) const;
    bool   HasBuffIdxGate(int idx, double limitSq) const;
    double HasBuffValueIdxGate(int idx, double limitSq) const;
    bool   PathContainsIdx(int idx) const;

private:
    // Detect which expensive lookups are referenced by the compiled string.
    void ComputeNeedsFlags();

    std::string exprString_;
    std::string compiledString_;

    // Pre-interned needles passed by index to EXPRTK functions.
    // pathNeedlesLower_ is pre-lowercased to skip per-frame work in PathContainsIdx.
    std::vector<std::string> buffNeedles_;
    std::vector<std::string> pathNeedles_;
    std::vector<std::string> pathNeedlesLower_;

    std::unique_ptr<exprtk::expression<double>> expression_;
    std::unique_ptr<exprtk::symbol_table<double>> symbolTable_;
    std::unique_ptr<exprtk::parser<double>> parser_;

    // --- Per-evaluation bound variables ---
    mutable double e_Id;
    mutable double e_IsValid;
    mutable double e_Rarity;
    mutable double e_EntityState;
    mutable double e_GridPositionX;
    mutable double e_GridPositionY;
    mutable double e_WorldX;
    mutable double e_WorldY;
    mutable double e_WorldZ;
    mutable double e_CurrentHP;
    mutable double e_MaxHP;
    mutable double e_CurrentES;
    mutable double e_MaxES;
    mutable double e_IsSleeping;
    mutable double e_CursorDistPx;
    // Squared cursor distance in pixels (avoids sqrt in common filters).
    mutable double e_CursorDistSq;
    mutable double e_Reaction;

    // --- Per-evaluation context for thunks ---
    mutable PluginContext* curCtx_ = nullptr;
    mutable const PluginSDK::RadarEntity* curEnt_ = nullptr;

    // Per-evaluation result caches for repeated needle lookups in one expr.
    mutable std::vector<int8_t>  buffResultCache_;   // -1 unknown, 0 false, 1 true
    mutable std::vector<int16_t> buffValueCache_;    // -32768 unknown, else value
    mutable std::vector<int8_t>  pathResultCache_;   // -1 unknown, 0 false, 1 true

    // Per-evaluation lazily-built lowered path of the current entity.
    mutable std::string pathLowerScratch_;
    mutable bool        pathLowerReady_ = false;

    // Per-evaluation cached Buffs component (avoids repeated ReadBuffsComponent calls).
    mutable bool                       buffsCacheReady_ = false;
    mutable bool                       buffsCacheUsedFallback_ = false;
    mutable bool                       buffsCacheValid_ = false;
    mutable PluginSDK::PluginBuffsData buffsCacheData_{};

    // --- "Needs" flags: avoid host-bridge calls when the expression doesn't
    //     actually reference these fields/functions. Set during Compile. ---
    bool needsCursorPx_  = false;
    bool needsCursorSq_  = false;
    bool needsBuffs_     = false;   // hasBuffIdx / hasBuffValueIdx in expr
    bool needsPath_      = false;   // pathContainsIdx in expr
    bool needsCursorForBuffGate_ = false; // hasBuff*Gate(...) requires cursor projection

    const PluginSDK::PluginBuffsData* GetBuffsDataCached(bool& outUsedFallback) const;
};

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    /**
     * Initializes the script engine subsystem.
     *
     * @returns True when initialization succeeded; otherwise false.
     */
    bool InitializeScriptEngineSubsystem();

    /** Returns whether the script engine subsystem has been initialized. */
    bool HasInitializedScriptEngineSubsystem() const { return hasInitializedScriptEngineSubsystem_; }

    // Debugging: override where AutoReflex writes the buffs dump log.
    // This is a global path used by the expression engine; intended to be set
    // by the plugin once from SetPluginDirectory().
    /**
     * Sets the output path for optional buffs dump logging.
     *
     * @param buffsDumpFilePath Full path to the dump file.
     * @returns None.
     */
    static void SetBuffsDumpPath(const std::string& buffsDumpFilePath);

    /**
     * Enables or disables optional buffs dump logging.
     *
     * @param isBuffsDumpEnabled True to enable logging; otherwise false.
     * @returns None.
     */
    static void SetBuffsDumpEnabled(bool isBuffsDumpEnabled);

    /**
     * Validates whether a raw user expression can be compiled.
     *
     * @param rawExpressionString User-authored script body.
     * @param outErrorMessage On failure, receives a user-facing error message.
     * @returns True when the expression validates; otherwise false.
     */
    static bool ValidateUserExpressionString(
        const std::string& rawExpressionString,
        std::string& outErrorMessage);

private:
    bool hasInitializedScriptEngineSubsystem_ = false;
};
