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
#include "sdk/PluginGameData.h"

#include "exprtk.hpp"

struct PluginContext;

class CompiledExpression {
public:
    CompiledExpression();
    ~CompiledExpression();

    // Compile an expression string; returns true on success.
    bool Compile(const std::string& exprString, std::string& errorMsg);

    // Evaluate against a single RadarEntity. Reads only the host-bridge data
    // referenced by the compiled expression (see needs* flags below).
    bool Evaluate(PluginContext* ctx,
                  const PluginSDK::RadarEntity& entity) const;

    bool IsValid() const { return expression_ != nullptr; }

    // Internal helpers used by EXPRTK thunks.
    bool   HasBuffIdx(int idx) const;
    double HasBuffValueIdx(int idx) const;
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

    // --- "Needs" flags: avoid host-bridge calls when the expression doesn't
    //     actually reference these fields/functions. Set during Compile. ---
    bool needsCursorPx_  = false;
    bool needsBuffs_     = false;   // hasBuffIdx / hasBuffValueIdx in expr
    bool needsPath_      = false;   // pathContainsIdx in expr
};

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    bool Initialize();
    bool IsInitialized() const { return initialized_; }

    static bool ValidateExpression(const std::string& expr, std::string& errorMsg);

private:
    bool initialized_ = false;
};
