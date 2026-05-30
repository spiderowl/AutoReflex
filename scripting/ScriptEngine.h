// AutoReflex - ScriptEngine.h
// EXPRTK-based expression evaluation engine.

#pragma once

#include "sdk/PluginSDK.h"

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

#include "exprtk.hpp"

class CompiledExpression {
public:
    CompiledExpression();
    ~CompiledExpression();

    bool CompileExpressionString(const std::string& rawExpressionString, std::string& outErrorMessage);

    bool EvaluateExpressionAgainstEntity(
        const PluginSDK::Context& pluginContext,
        const PluginSDK::Entity& entity) const;

    bool EvaluatePlayerCondition(const PluginSDK::Context& pluginContext) const;

    bool HasCompiledExpression() const { return expression_ != nullptr; }

    bool   HasBuffIdx(int idx) const;
    double HasBuffValueIdx(int idx) const;
    bool   HasBuffIdxGate(int idx, double limitSq) const;
    double HasBuffValueIdxGate(int idx, double limitSq) const;
    bool   PathContainsIdx(int idx) const;

private:
    void ComputeNeedsFlags();
    void BindPlayerVitalsFromTickCache() const;

    std::string exprString_;
    std::string compiledString_;

    std::vector<std::string> buffNeedles_;
    std::vector<std::string> pathNeedles_;
    std::vector<std::string> pathNeedlesLower_;

    std::unique_ptr<exprtk::expression<double>> expression_;
    std::unique_ptr<exprtk::symbol_table<double>> symbolTable_;
    std::unique_ptr<exprtk::parser<double>> parser_;

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
    mutable double e_CursorDistSq;
    mutable double e_Reaction;

    mutable double p_HPPercent;
    mutable double p_ESPercent;
    mutable double p_MPPercent;
    mutable double p_CurrentHP;
    mutable double p_MaxHP;
    mutable double p_CurrentES;
    mutable double p_MaxES;
    mutable double p_CurrentMP;
    mutable double p_MaxMP;

    mutable const PluginSDK::Context* curCtx_ = nullptr;
    mutable const PluginSDK::Entity* curEnt_ = nullptr;

    mutable std::vector<int8_t>  buffResultCache_;
    mutable std::vector<int16_t> buffValueCache_;
    mutable std::vector<int8_t>  pathResultCache_;

    mutable std::string pathLowerScratch_;
    mutable bool        pathLowerReady_ = false;

    mutable bool                       buffsCacheReady_ = false;
    mutable std::vector<PluginSDK::Buff> buffsCache_;

    bool needsCursorPx_  = false;
    bool needsCursorSq_  = false;
    bool needsBuffs_     = false;
    bool needsPath_      = false;
    bool needsCursorForBuffGate_ = false;

    const std::vector<PluginSDK::Buff>* GetBuffsDataCached() const;
};

class ScriptEngine {
public:
    static void SetBuffsDumpPath(const std::string& buffsDumpFilePath);
    static void SetBuffsDumpEnabled(bool isBuffsDumpEnabled);
};
