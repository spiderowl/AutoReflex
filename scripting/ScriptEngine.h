// AutoReflex - ScriptEngine.h
// Pure AngelScript 2.38.0 integration (no asbind20 dependency)
// Uses PluginSDK::RadarEntity directly instead of custom EntityInfo

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <memory>

#include "angelscript.h"
#include "sdk/PluginGameData.h"

// Callback aliases using SDK's RadarEntity directly
using ConditionCallback = std::function<bool(const PluginSDK::RadarEntity&)>;
using ActionCallback = std::function<void(const PluginSDK::RadarEntity&)>;

// ============================================================================
// ScriptEngine - manages AngelScript engine lifecycle
// ============================================================================

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    // Initialize the AngelScript engine (call once on plugin load)
    bool Initialize();

    // Compile a script string; returns true on success
    bool CompileScript(const std::string& script, std::string& errorMsg);

    // Get a condition callback for entity evaluation
    ConditionCallback GetCondition(const std::string& funcName);

    // Get an action callback for entity execution
    ActionCallback GetAction(const std::string& funcName);

    // Check if the engine is ready
    bool IsInitialized() const { return engine != nullptr; }

    // Get the raw AngelScript engine pointer
    asIScriptEngine* GetEngine() const { return engine; }

    // Get last error message
    const std::string& GetLastError() const { return lastError; }

private:
    asIScriptEngine* engine = nullptr;
    asIScriptModule* module = nullptr;
    asIScriptContext* context = nullptr;
    std::string lastError;

    // Manual registration
    void RegisterTypes();
    void RegisterStdlib();

    // Bound callbacks
    struct BoundCallback {
        std::string funcName;
        asIScriptFunction* function = nullptr;
        bool isCondition = false;
    };
    std::vector<BoundCallback> boundCallbacks;
};

// Global singleton access
ScriptEngine& GetScriptEngine();