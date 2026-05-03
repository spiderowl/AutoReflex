// AutoReflex - ScriptEngine.cpp
// Pure AngelScript 2.38.0 manual registration (no asbind20)
// Registers PluginSDK::RadarEntity directly for script access

#include "ScriptEngine.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <cstring>
#include <cstdio>

// ============================================================================
// ScriptEngine implementation
// ============================================================================

ScriptEngine::ScriptEngine() {}

ScriptEngine::~ScriptEngine() {
    if (engine) {
        engine->ShutDownAndRelease();
        engine = nullptr;
    }
}

bool ScriptEngine::Initialize() {
    lastError.clear();

    // Create engine
    engine = asCreateScriptEngine();
    if (!engine) {
        lastError = "Failed to create AngelScript engine";
        return false;
    }

    // Register types
    RegisterTypes();

    // Create module
    module = engine->GetModule("AutoReflex", asGM_CREATE_IF_NOT_EXISTS);
    if (!module) {
        lastError = "Failed to create script module";
        engine->ShutDownAndRelease();
        engine = nullptr;
        return false;
    }

    // Register stdlib
    RegisterStdlib();

    return true;
}

void ScriptEngine::RegisterTypes() {
    if (!engine) return;

    int r;

    // Register RadarEntity struct (key script-accessible fields from SDK)
    r = engine->RegisterObjectType("RadarEntity", sizeof(PluginSDK::RadarEntity), asOBJ_VALUE | asOBJ_APP_CLASS_CDA);
    assert(r >= 0);

    // Id
    r = engine->RegisterObjectProperty("RadarEntity", "uint Id", asOFFSET(PluginSDK::RadarEntity, Id));
    assert(r >= 0);

    // IsValid
    r = engine->RegisterObjectProperty("RadarEntity", "bool IsValid", asOFFSET(PluginSDK::RadarEntity, IsValid));
    assert(r >= 0);

    // Rarity
    r = engine->RegisterObjectProperty("RadarEntity", "int Rarity", asOFFSET(PluginSDK::RadarEntity, Rarity));
    assert(r >= 0);

    // Position
    r = engine->RegisterObjectProperty("RadarEntity", "float GridPositionX", asOFFSET(PluginSDK::RadarEntity, GridPositionX));
    assert(r >= 0);
    r = engine->RegisterObjectProperty("RadarEntity", "float GridPositionY", asOFFSET(PluginSDK::RadarEntity, GridPositionY));
    assert(r >= 0);
    r = engine->RegisterObjectProperty("RadarEntity", "float WorldX", asOFFSET(PluginSDK::RadarEntity, WorldX));
    assert(r >= 0);
    r = engine->RegisterObjectProperty("RadarEntity", "float WorldY", asOFFSET(PluginSDK::RadarEntity, WorldY));
    assert(r >= 0);
    r = engine->RegisterObjectProperty("RadarEntity", "float WorldZ", asOFFSET(PluginSDK::RadarEntity, WorldZ));
    assert(r >= 0);

    // HP
    r = engine->RegisterObjectProperty("RadarEntity", "int CurrentHP", asOFFSET(PluginSDK::RadarEntity, CurrentHP));
    assert(r >= 0);
    r = engine->RegisterObjectProperty("RadarEntity", "int MaxHP", asOFFSET(PluginSDK::RadarEntity, MaxHP));
    assert(r >= 0);

    // ES
    r = engine->RegisterObjectProperty("RadarEntity", "int CurrentES", asOFFSET(PluginSDK::RadarEntity, CurrentES));
    assert(r >= 0);
    r = engine->RegisterObjectProperty("RadarEntity", "int MaxES", asOFFSET(PluginSDK::RadarEntity, MaxES));
    assert(r >= 0);

    // State flags
    r = engine->RegisterObjectProperty("RadarEntity", "bool IsSleeping", asOFFSET(PluginSDK::RadarEntity, IsSleeping));
    assert(r >= 0);

    // TgtPath (std::string registered as string)
    r = engine->RegisterObjectProperty("RadarEntity", "string TgtPath", asOFFSET(PluginSDK::RadarEntity, TgtPath));
    assert(r >= 0);
}

void ScriptEngine::RegisterStdlib() {
    // Minimal stdlib - just the essentials
    const char* stdlib =
        "import void print(string)\n"
        "import int len(string)\n"
        "import int len(array<string>)\n";

    if (module) {
        int r = module->AddScriptSection("stdlib", stdlib, strlen(stdlib), 0);
        if (r >= 0) {
            r = module->Build();
        }
    }
}

bool ScriptEngine::CompileScript(const std::string& script, std::string& errorMsg) {
    if (!module) {
        errorMsg = "Engine not initialized";
        return false;
    }

    int r = module->AddScriptSection("user_script", script.c_str(), script.length(), 0);
    if (r < 0) {
        errorMsg = "Failed to add script section";
        return false;
    }

    r = module->Build();
    if (r < 0) {
        errorMsg = "Script compilation failed";
        return false;
    }

    return true;
}

ConditionCallback ScriptEngine::GetCondition(const std::string& funcName) {
    if (!module) return nullptr;

    asIScriptFunction* func = module->GetFunctionByName(funcName.c_str());
    if (!func) return nullptr;

    asIScriptEngine* eng = engine;
    asIScriptFunction* fn = func;

    return [eng, fn](const PluginSDK::RadarEntity& e) -> bool {
        asIScriptContext* ctx = eng->CreateContext();
        if (!ctx) return false;

        ctx->SetException("Script execution failed");
        ctx->Release();
        return false;
    };
}

ActionCallback ScriptEngine::GetAction(const std::string& funcName) {
    if (!module) return nullptr;

    asIScriptFunction* func = module->GetFunctionByName(funcName.c_str());
    if (!func) return nullptr;

    asIScriptEngine* eng = engine;
    asIScriptFunction* fn = func;

    return [eng, fn](const PluginSDK::RadarEntity& e) {
        asIScriptContext* ctx = eng->CreateContext();
        if (!ctx) return;

        ctx->SetException("Script execution failed");
        ctx->Release();
    };
}

// ============================================================================
// Global singleton
// ============================================================================

ScriptEngine& GetScriptEngine() {
    static ScriptEngine instance;
    return instance;
}