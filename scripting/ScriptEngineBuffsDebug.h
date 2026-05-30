// AutoReflex - ScriptEngineBuffsDebug.h

#pragma once

#include "sdk/PluginSDK.h"

#include <cstdint>
#include <string>
#include <vector>

namespace AutoReflex::Scripting::Internal {

void AppendBuffsDebugDumpLine(
    const PluginSDK::Entity& entity,
    const std::vector<PluginSDK::Buff>* buffsOrNull,
    const char* tag);

uintptr_t ResolveBuffsComponentAddress(
    const PluginSDK::Entity& entity);

void SetBuffsDebugDumpFilePath(const std::string& buffsDumpFilePath);

void SetBuffsDebugDumpEnabled(bool isBuffsDebugDumpEnabled);

bool GetIsBuffsDebugDumpEnabled();

} // namespace AutoReflex::Scripting::Internal
