// AutoReflex - ScriptEngineBuffsDebug
// Optional buffs dump logging and Buffs component address resolution helpers.

#pragma once

#include "../sdk/PluginGameData.h"

#include <cstdint>
#include <string>

struct PluginContext;

namespace AutoReflex::Scripting::Internal {

/**
 * Appends a single debug dump line for an entity's buffs state.
 *
 * @param radarEntity Entity being evaluated.
 * @param buffsDataOrNull Buffs data when available; otherwise null.
 * @param tag Short tag describing the callsite (e.g., Evaluate_TRUE).
 * @returns None.
 */
void AppendBuffsDebugDumpLine(
    const PluginSDK::RadarEntity& radarEntity,
    const PluginSDK::PluginBuffsData* buffsDataOrNull,
    const char* tag);

/**
 * Resolves an entity's Buffs component address using the fast cache first, then a debug-list fallback.
 *
 * @param pluginContext Host context providing bridge calls and debug list.
 * @param radarEntity Entity whose buffs address is needed.
 * @param outUsedFallback True when fallback was used; otherwise false.
 * @returns Resolved address, or 0 when unavailable.
 */
uintptr_t ResolveBuffsComponentAddress(
    PluginContext* pluginContext,
    const PluginSDK::RadarEntity& radarEntity,
    bool& outUsedFallback);

/**
 * Sets the output file path for buffs dump logging (only used when compile-time enabled).
 *
 * @param buffsDumpFilePath Full path to the dump file.
 * @returns None.
 */
void SetBuffsDebugDumpFilePath(const std::string& buffsDumpFilePath);

/**
 * Enables or disables buffs dump logging (only used when compile-time enabled).
 *
 * @param isBuffsDebugDumpEnabled True to enable logging; otherwise false.
 * @returns None.
 */
void SetBuffsDebugDumpEnabled(bool isBuffsDebugDumpEnabled);

/**
 * Returns whether buffs dump logging is enabled (compile-time + runtime).
 *
 * @returns True when enabled; otherwise false.
 */
bool GetIsBuffsDebugDumpEnabled();

} // namespace AutoReflex::Scripting::Internal

