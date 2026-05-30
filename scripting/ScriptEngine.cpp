// AutoReflex - ScriptEngine.cpp

#include "ScriptEngine.h"
#include "../core/EvalTickCache.h"
#include "../game/MonsterHelpers.h"
#include "ScriptEngineDslPreprocessor.h"
#include "ScriptEngineBuffsDebug.h"

#include "exprtk.hpp"

#include <imgui.h>

#include <cmath>
#include <string>
#include <algorithm>

using symbol_table_t = exprtk::symbol_table<double>;
using expression_t   = exprtk::expression<double>;
using parser_t       = exprtk::parser<double>;

CompiledExpression::CompiledExpression()
    : expression_(std::make_unique<expression_t>())
    , symbolTable_(std::make_unique<symbol_table_t>())
    , parser_(std::make_unique<parser_t>())
{
    symbolTable_->add_variable("e_Id",            e_Id);
    symbolTable_->add_variable("e_IsValid",       e_IsValid);
    symbolTable_->add_variable("e_Rarity",        e_Rarity);
    symbolTable_->add_variable("e_EntityState",   e_EntityState);
    symbolTable_->add_variable("e_GridPositionX", e_GridPositionX);
    symbolTable_->add_variable("e_GridPositionY", e_GridPositionY);
    symbolTable_->add_variable("e_WorldX",        e_WorldX);
    symbolTable_->add_variable("e_WorldY",        e_WorldY);
    symbolTable_->add_variable("e_WorldZ",        e_WorldZ);
    symbolTable_->add_variable("e_CurrentHP",     e_CurrentHP);
    symbolTable_->add_variable("e_MaxHP",         e_MaxHP);
    symbolTable_->add_variable("e_CurrentES",     e_CurrentES);
    symbolTable_->add_variable("e_MaxES",         e_MaxES);
    symbolTable_->add_variable("e_IsSleeping",    e_IsSleeping);
    symbolTable_->add_variable("e_CursorDistPx",  e_CursorDistPx);
    symbolTable_->add_variable("e_CursorDistSq",  e_CursorDistSq);
    symbolTable_->add_variable("e_Reaction",      e_Reaction);

    symbolTable_->add_variable("p_HPPercent",  p_HPPercent);
    symbolTable_->add_variable("p_ESPercent",  p_ESPercent);
    symbolTable_->add_variable("p_MPPercent",  p_MPPercent);
    symbolTable_->add_variable("p_CurrentHP", p_CurrentHP);
    symbolTable_->add_variable("p_MaxHP",     p_MaxHP);
    symbolTable_->add_variable("p_CurrentES", p_CurrentES);
    symbolTable_->add_variable("p_MaxES",     p_MaxES);
    symbolTable_->add_variable("p_CurrentMP", p_CurrentMP);
    symbolTable_->add_variable("p_MaxMP",     p_MaxMP);

    symbolTable_->add_constants();
    expression_->register_symbol_table(*symbolTable_);
}

CompiledExpression::~CompiledExpression() = default;

namespace {

thread_local const CompiledExpression* tl_expr = nullptr;

double HasBuffIdxThunk(double idx) {
    if (!tl_expr) return 0.0;
    return tl_expr->HasBuffIdx(static_cast<int>(idx)) ? 1.0 : 0.0;
}
double HasBuffValueIdxThunk(double idx) {
    if (!tl_expr) return 0.0;
    return tl_expr->HasBuffValueIdx(static_cast<int>(idx));
}
double HasBuffIdxGateThunk(double idx, double limitSq) {
    if (!tl_expr) return 0.0;
    return tl_expr->HasBuffIdxGate(static_cast<int>(idx), limitSq) ? 1.0 : 0.0;
}
double HasBuffValueIdxGateThunk(double idx, double limitSq) {
    if (!tl_expr) return 0.0;
    return tl_expr->HasBuffValueIdxGate(static_cast<int>(idx), limitSq);
}
double PathContainsIdxThunk(double idx) {
    if (!tl_expr) return 0.0;
    return tl_expr->PathContainsIdx(static_cast<int>(idx)) ? 1.0 : 0.0;
}

inline char AsciiToLower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
}

void LowerAsciiInPlace(std::string& s) {
    for (char& c : s) c = AsciiToLower(c);
}

bool Contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

} // anonymous namespace

void ScriptEngine::SetBuffsDumpPath(const std::string& path)
{
    AutoReflex::Scripting::Internal::SetBuffsDebugDumpFilePath(path);
}

void ScriptEngine::SetBuffsDumpEnabled(bool enabled)
{
    AutoReflex::Scripting::Internal::SetBuffsDebugDumpEnabled(enabled);
}

void CompiledExpression::BindPlayerVitalsFromTickCache() const
{
    if (const AutoReflex::Core::EvalTickCache* tickCache = AutoReflex::Core::g_ActiveEvalTickCache) {
        p_HPPercent  = static_cast<double>(tickCache->PlayerHPPercent());
        p_ESPercent  = static_cast<double>(tickCache->PlayerESPercent());
        p_MPPercent  = static_cast<double>(tickCache->PlayerMPPercent());
        p_CurrentHP  = static_cast<double>(tickCache->PlayerCurrentHP());
        p_MaxHP      = static_cast<double>(tickCache->PlayerMaxHP());
        p_CurrentES  = static_cast<double>(tickCache->PlayerCurrentES());
        p_MaxES      = static_cast<double>(tickCache->PlayerMaxES());
        p_CurrentMP  = static_cast<double>(tickCache->PlayerCurrentMP());
        p_MaxMP      = static_cast<double>(tickCache->PlayerMaxMP());
        return;
    }

    p_HPPercent = 100.0;
    p_ESPercent = 100.0;
    p_MPPercent = 100.0;
    p_CurrentHP = 0.0;
    p_MaxHP = 0.0;
    p_CurrentES = 0.0;
    p_MaxES = 0.0;
    p_CurrentMP = 0.0;
    p_MaxMP = 0.0;
}

bool CompiledExpression::EvaluatePlayerCondition(
    const PluginSDK::Context& pluginContext) const
{
    if (!expression_) return false;

    tl_expr = this;
    curCtx_ = &pluginContext;
    curEnt_ = nullptr;
    BindPlayerVitalsFromTickCache();

    return expression_->value() != 0.0;
}

void CompiledExpression::ComputeNeedsFlags()
{
    needsCursorPx_   = compiledString_.find("e_CursorDistPx")  != std::string::npos;
    needsCursorSq_   = compiledString_.find("e_CursorDistSq")  != std::string::npos;
    needsBuffs_      = (compiledString_.find("hasBuffIdx(")          != std::string::npos)
                    || (compiledString_.find("hasBuffValueIdx(")     != std::string::npos)
                    || (compiledString_.find("hasBuffIdxGate(")      != std::string::npos)
                    || (compiledString_.find("hasBuffValueIdxGate(") != std::string::npos);
    needsPath_       = compiledString_.find("pathContainsIdx(") != std::string::npos;
    needsCursorForBuffGate_ = (compiledString_.find("hasBuffIdxGate(") != std::string::npos)
                           || (compiledString_.find("hasBuffValueIdxGate(") != std::string::npos);
}

bool CompiledExpression::CompileExpressionString(
    const std::string& rawExpressionString,
    std::string& outErrorMessage)
{
    exprString_ = rawExpressionString;
    compiledString_.clear();
    buffNeedles_.clear();
    pathNeedles_.clear();
    pathNeedlesLower_.clear();

    if (!AutoReflex::Scripting::Internal::PreprocessUserExpressionStringToExprtkExpressionString(
            exprString_, compiledString_, buffNeedles_, pathNeedles_, outErrorMessage)) {
        return false;
    }

    pathNeedlesLower_.reserve(pathNeedles_.size());
    for (const auto& n : pathNeedles_) {
        std::string lower = n;
        LowerAsciiInPlace(lower);
        pathNeedlesLower_.push_back(std::move(lower));
    }

    symbolTable_->add_function("hasBuffIdx",      &HasBuffIdxThunk);
    symbolTable_->add_function("hasBuffValueIdx", &HasBuffValueIdxThunk);
    symbolTable_->add_function("hasBuffIdxGate",      &HasBuffIdxGateThunk);
    symbolTable_->add_function("hasBuffValueIdxGate", &HasBuffValueIdxGateThunk);
    symbolTable_->add_function("pathContainsIdx", &PathContainsIdxThunk);

    if (!parser_->compile(compiledString_, *expression_)) {
        outErrorMessage = "Expression compilation failed. Translated: " + compiledString_;
        return false;
    }

    ComputeNeedsFlags();
    outErrorMessage.clear();
    return true;
}

bool CompiledExpression::EvaluateExpressionAgainstEntity(
    const PluginSDK::Context& pluginContext,
    const PluginSDK::Entity& entity) const
{
    if (!expression_) return false;

    tl_expr = this;

    e_Id            = static_cast<double>(entity.Id);
    e_IsValid       = entity.IsValid ? 1.0 : 0.0;
    e_Rarity        = static_cast<double>(entity.Rarity);
    e_EntityState   = static_cast<double>(static_cast<int>(entity.EntityState));
    e_GridPositionX = static_cast<double>(entity.GridPositionX);
    e_GridPositionY = static_cast<double>(entity.GridPositionY);
    e_WorldX        = static_cast<double>(entity.WorldX);
    e_WorldY        = static_cast<double>(entity.WorldY);
    e_WorldZ        = static_cast<double>(entity.WorldZ);
    e_CurrentHP     = static_cast<double>(entity.CurrentHP);
    e_MaxHP         = static_cast<double>(entity.MaxHP);
    e_CurrentES     = static_cast<double>(entity.CurrentES);
    e_MaxES         = static_cast<double>(entity.MaxES);
    e_IsSleeping    = entity.IsSleeping ? 1.0 : 0.0;
    e_Reaction      = static_cast<double>(entity.Reaction);

    curCtx_ = &pluginContext;
    curEnt_ = &entity;
    BindPlayerVitalsFromTickCache();
    buffsCacheReady_ = false;
    buffsCache_.clear();

    const std::vector<PluginSDK::Buff>* dbgPtr = nullptr;
    const bool dumpEnabled = AutoReflex::Scripting::Internal::GetIsBuffsDebugDumpEnabled();

    if (needsBuffs_) {
        buffResultCache_.assign(buffNeedles_.size(), -1);
        buffValueCache_.assign(buffNeedles_.size(), static_cast<int16_t>(-32768));
    }
    if (needsPath_) {
        pathResultCache_.assign(pathNeedles_.size(), -1);
        pathLowerReady_ = false;
    }

    if (needsCursorPx_ || needsCursorSq_ || needsCursorForBuffGate_) {
        if (AutoReflex::Core::g_ActiveEvalTickCache) {
            AutoReflex::Core::g_ActiveEvalTickCache->GetOrComputeCursorDistance(
                pluginContext,
                entity,
                needsCursorPx_,
                e_CursorDistSq,
                e_CursorDistPx);
        } else {
            float screenX = 0.f;
            float screenY = 0.f;
            if (pluginContext.Render.WorldToScreen(
                    entity.WorldX, entity.WorldY, entity.WorldZ, screenX, screenY)) {
                const ImVec2 mouse = ImGui::GetMousePos();
                const float deltaX = screenX - mouse.x;
                const float deltaY = screenY - mouse.y;
                e_CursorDistSq = static_cast<double>(deltaX * deltaX + deltaY * deltaY);
                if (needsCursorPx_) {
                    e_CursorDistPx = std::sqrt(e_CursorDistSq);
                }
            } else {
                e_CursorDistPx = 1.0e9;
                e_CursorDistSq = 1.0e18;
            }
        }
    } else {
        e_CursorDistPx = 1.0e9;
        e_CursorDistSq = 1.0e18;
    }

    if (dumpEnabled && needsBuffs_) {
        dbgPtr = GetBuffsDataCached();
        AutoReflex::Scripting::Internal::AppendBuffsDebugDumpLine(
            entity,
            dbgPtr,
            dbgPtr ? "Evaluate" : "Evaluate_NoBuffsAddr");
    }

    const double result = expression_->value();

    if (dumpEnabled && result != 0.0) {
        if (!dbgPtr && needsBuffs_) {
            dbgPtr = GetBuffsDataCached();
        }
        AutoReflex::Scripting::Internal::AppendBuffsDebugDumpLine(
            entity, dbgPtr, "Evaluate_TRUE");
    }

    return result != 0.0;
}

const std::vector<PluginSDK::Buff>* CompiledExpression::GetBuffsDataCached() const
{
    if (!curCtx_ || !curEnt_) return nullptr;

    if (!buffsCacheReady_) {
        buffsCacheReady_ = true;
        buffsCache_.clear();

        const uintptr_t buffsAddr =
            AutoReflex::Scripting::Internal::ResolveBuffsComponentAddress(*curEnt_);
        if (buffsAddr == 0) {
            return &buffsCache_;
        }

        if (AutoReflex::Core::g_ActiveEvalTickCache) {
            const auto* sharedBuffs =
                AutoReflex::Core::g_ActiveEvalTickCache->GetOrLoadBuffs(*curCtx_, buffsAddr);
            if (sharedBuffs) {
                buffsCache_ = *sharedBuffs;
            }
        } else {
            buffsCache_ = curCtx_->Components.EnumerateBuffs(buffsAddr);
        }
    }

    return &buffsCache_;
}

bool CompiledExpression::HasBuffIdx(int idx) const
{
    if (!curCtx_ || !curEnt_) return false;
    if (idx < 0 || idx >= static_cast<int>(buffNeedles_.size())) return false;

    if (idx < static_cast<int>(buffResultCache_.size()) && buffResultCache_[idx] != -1)
        return buffResultCache_[idx] == 1;

    const auto* buffs = GetBuffsDataCached();
    if (!buffs || buffs->empty()) {
        AutoReflex::Scripting::Internal::AppendBuffsDebugDumpLine(*curEnt_, nullptr, "ReadBuffsComponent_NoAddr");
        if (idx < static_cast<int>(buffResultCache_.size())) buffResultCache_[idx] = 0;
        return false;
    }
    AutoReflex::Scripting::Internal::AppendBuffsDebugDumpLine(
        *curEnt_, buffs, "ReadBuffsComponent");

    bool found = false;
    const auto& needle = buffNeedles_[idx];
    for (const auto& b : *buffs) {
        if (b.Name == needle) { found = true; break; }
    }
    if (idx < static_cast<int>(buffResultCache_.size())) buffResultCache_[idx] = found ? 1 : 0;
    return found;
}

bool CompiledExpression::HasBuffIdxGate(int idx, double limitSq) const
{
    if (e_CursorDistSq > limitSq) return false;
    return HasBuffIdx(idx);
}

double CompiledExpression::HasBuffValueIdx(int idx) const
{
    if (!curCtx_ || !curEnt_) return 0.0;
    if (idx < 0 || idx >= static_cast<int>(buffNeedles_.size())) return 0.0;

    if (idx < static_cast<int>(buffValueCache_.size()) && buffValueCache_[idx] != static_cast<int16_t>(-32768))
        return static_cast<double>(buffValueCache_[idx]);

    const auto* buffs = GetBuffsDataCached();
    if (!buffs || buffs->empty()) {
        AutoReflex::Scripting::Internal::AppendBuffsDebugDumpLine(*curEnt_, nullptr, "ReadBuffsComponent_NoAddr");
        if (idx < static_cast<int>(buffValueCache_.size())) buffValueCache_[idx] = 0;
        return 0.0;
    }
    AutoReflex::Scripting::Internal::AppendBuffsDebugDumpLine(
        *curEnt_, buffs, "ReadBuffsComponent");

    int16_t value = 0;
    const auto& needle = buffNeedles_[idx];
    for (const auto& b : *buffs) {
        if (b.Name == needle) { value = b.Charges; break; }
    }
    if (idx < static_cast<int>(buffValueCache_.size())) buffValueCache_[idx] = value;
    return static_cast<double>(value);
}

double CompiledExpression::HasBuffValueIdxGate(int idx, double limitSq) const
{
    if (e_CursorDistSq > limitSq) return 0.0;
    return HasBuffValueIdx(idx);
}

bool CompiledExpression::PathContainsIdx(int idx) const
{
    if (!curEnt_) return false;
    if (idx < 0 || idx >= static_cast<int>(pathNeedlesLower_.size())) return false;

    if (idx < static_cast<int>(pathResultCache_.size()) && pathResultCache_[idx] != -1)
        return pathResultCache_[idx] == 1;

    if (!pathLowerReady_) {
        pathLowerScratch_ = AutoReflex::Game::ConvertWideStringUtf16ToUtf8String(curEnt_->Path);
        LowerAsciiInPlace(pathLowerScratch_);
        pathLowerReady_ = true;
    }

    const bool ok = Contains(pathLowerScratch_, pathNeedlesLower_[idx]);
    if (idx < static_cast<int>(pathResultCache_.size())) pathResultCache_[idx] = ok ? 1 : 0;
    return ok;
}
