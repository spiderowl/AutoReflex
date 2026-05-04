// AutoReflex - RuleManager.cpp
// Compiles and evaluates rules using AngelScript

#include "RuleManager.h"
#include "Rule.h"
#include "../game/ConditionState.h"
#include "../storage/RuleStore.h"
#include "../sdk/PluginContext.h"

#include "angelscript.h"

#include <chrono>
#include <sstream>
#include <algorithm>
#include <numeric>

namespace AutoReflex { namespace Rules {

RuleManager::RuleManager(asIScriptEngine* engine)
    : m_Engine(engine)
{
}

RuleManager::~RuleManager()
{
}

void RuleManager::LoadRules(Storage::RuleStore& store)
{
    // Load all rules from disk into m_Rules
    store.LoadAll(m_Rules);

    // Compile each loaded rule
    for (auto& rule : m_Rules) {
        CompileRule(rule);
    }
}

void RuleManager::CompileRule(Rule& rule)
{
    if (!m_Engine) {
        rule.CompileError = "Script engine not initialized";
        rule.Module = nullptr;
        return;
    }

    // Build module name from rule name: "Rule_<name>"
    std::string moduleName = "Rule_";
    for (char c : rule.Name) {
        if (std::isalnum(static_cast<unsigned char>(c))) moduleName += c;
        else moduleName += '_';
    }

    // Get or create module
    asIScriptModule* module = m_Engine->GetModule(moduleName.c_str(), asGM_CREATE_IF_NOT_EXISTS);
    if (!module) {
        rule.CompileError = "Failed to create script module";
        return;
    }

    // Combine boilerplate + user body
    std::string fullScript = "// AutoReflex rule script\n" + rule.ScriptBody;

    // Build the script
    int r = module->Build();
    if (r < 0) {
        // AngelScript doesn't expose per-module error messages easily in this SDK
        rule.CompileError = "Compilation failed (check script syntax)";
        return;
    }

    rule.Module = module;
    rule.CompileError.clear();
}

void RuleManager::EvaluateAll(
    PluginContext* ctx,
    Game::ConditionState& conditionState,
    std::function<void(const Rule&)> onFire)
{
    auto now = std::chrono::steady_clock::now();

    // Sort by order (lower = higher priority)
    std::vector<size_t> indices(m_Rules.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return m_Rules[a].Order < m_Rules[b].Order;
    });

    for (size_t idx : indices) {
        Rule& rule = m_Rules[idx];

        // Skip disabled rules
        if (!rule.Enabled) continue;

        // Skip if no compiled module
        if (!rule.Module) continue;

        // Check cooldown
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - rule.LastFired).count();
        if (elapsed < rule.CooldownSec * 1000.0f) continue;

        // Execute AngelScript: call CheckCondition(RadarEntity& entity) for each monster
        bool conditionMet = false;

        if (rule.Module && m_Engine) {
            // Find the CheckCondition function in this rule's module
            asIScriptFunction* func = rule.Module->GetFunctionByName("CheckCondition");
            if (func) {
                // Create a script context
                asIScriptContext* ctx_script = m_Engine->CreateContext();
                if (ctx_script) {
                    int r = ctx_script->Prepare(func);
                    if (r >= 0) {
                        // Get Entities from PluginContext snapshot
                        auto snapshot = ctx->GetSnapshot();
                        if (snapshot) {
                            for (const auto& entity : snapshot->Entities) {
                                if (!entity.IsValid) continue;

                                // Pass entity as argument (RadarEntity is passed by pointer)
                                ctx_script->SetArgObject(0, const_cast<PluginSDK::RadarEntity*>(&entity));

                                r = ctx_script->Execute();
                                if (r >= 0) {
                                    // Read return value (bool returns as byte)
                                    bool result = ctx_script->GetReturnByte() != 0;
                                    if (result) {
                                        conditionMet = true;
                                        break;
                                    }
                                } else {
                                    // Execution failed, check exception
                                    const char* exception = ctx_script->GetExceptionString();
                                    (void)exception; // Silently skip entity on script error
                                }
                                ctx_script->Unprepare();
                            }
                        }
                    }

                    ctx_script->Release();
                }
            }
        }

        rule.LastEvalResult = conditionMet;

        // If condition is true, fire the rule
        if (conditionMet && onFire) {
            onFire(rule);
            rule.LastFired = now;
            rule.EverFired = true;
        }
    }
}

} } // namespace AutoReflex::Rules