// AutoReflex - ScriptEngineBuffsDebug.cpp

#include "ScriptEngineBuffsDebug.h"

#include "../sdk/PluginContext.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#if defined(_DEBUG) || defined(AUTOREFLEX_ENABLE_BUFFS_DUMP)
#define AUTOREFLEX_BUFFS_DUMP 1
#else
#define AUTOREFLEX_BUFFS_DUMP 0
#endif

namespace AutoReflex::Scripting::Internal {

namespace {

uintptr_t TryGetBuffsComponentAddressFromDebugList(PluginContext* pluginContext, uint32_t entityId)
{
    if (!pluginContext || !pluginContext->GetEntityDebugList) return 0;

    static std::mutex s_cacheMu;
    static std::unordered_map<uint32_t, uintptr_t> s_buffsComponentAddressByEntityId;
    static uint64_t s_lastRefreshMs = 0;

    const uint64_t nowMs =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());

    {
        std::lock_guard<std::mutex> lock(s_cacheMu);
        const auto it = s_buffsComponentAddressByEntityId.find(entityId);
        if (it != s_buffsComponentAddressByEntityId.end()) return it->second;
        if (nowMs - s_lastRefreshMs < 500) return 0;
    }

    const auto debugEntityList = pluginContext->GetEntityDebugList();
    if (debugEntityList.empty()) return 0;

    std::unordered_map<uint32_t, uintptr_t> newMapping;
    newMapping.reserve(debugEntityList.size());

    for (const auto& debugEntityInfo : debugEntityList) {
        uintptr_t buffsComponentAddress = 0;
        for (const auto& componentNameAndAddress : debugEntityInfo.ComponentAddresses) {
            const std::string& componentName = componentNameAndAddress.first;
            const uintptr_t componentAddress = componentNameAndAddress.second;
            if (componentAddress == 0) continue;
            if (componentName.find("Buff") != std::string::npos ||
                componentName.find("buff") != std::string::npos) {
                buffsComponentAddress = componentAddress;
                break;
            }
        }
        if (buffsComponentAddress != 0) {
            newMapping[debugEntityInfo.Id] = buffsComponentAddress;
        }
    }

    {
        std::lock_guard<std::mutex> lock(s_cacheMu);
        s_buffsComponentAddressByEntityId.swap(newMapping);
        s_lastRefreshMs = nowMs;
        const auto it = s_buffsComponentAddressByEntityId.find(entityId);
        if (it != s_buffsComponentAddressByEntityId.end()) return it->second;
    }

    return 0;
}

} // namespace

#if AUTOREFLEX_BUFFS_DUMP
namespace {

std::mutex g_buffsDumpMu;
std::string g_buffsDumpPath;
bool g_buffsDumpEnabled = false;

bool GetIsTrueMatchTag(const char* tag)
{
    if (!tag) return false;
    return std::string(tag).find("_TRUE") != std::string::npos;
}

} // namespace
#endif

void SetBuffsDebugDumpFilePath(const std::string& buffsDumpFilePath)
{
#if !AUTOREFLEX_BUFFS_DUMP
    (void)buffsDumpFilePath;
#else
    std::lock_guard<std::mutex> lock(g_buffsDumpMu);
    g_buffsDumpPath = buffsDumpFilePath;
#endif
}

void SetBuffsDebugDumpEnabled(bool isBuffsDebugDumpEnabled)
{
#if !AUTOREFLEX_BUFFS_DUMP
    (void)isBuffsDebugDumpEnabled;
#else
    std::lock_guard<std::mutex> lock(g_buffsDumpMu);
    g_buffsDumpEnabled = isBuffsDebugDumpEnabled;
#endif
}

bool GetIsBuffsDebugDumpEnabled()
{
#if !AUTOREFLEX_BUFFS_DUMP
    return false;
#else
    std::lock_guard<std::mutex> lock(g_buffsDumpMu);
    return g_buffsDumpEnabled;
#endif
}

uintptr_t ResolveBuffsComponentAddress(
    PluginContext* pluginContext,
    const PluginSDK::RadarEntity& radarEntity,
    bool& outUsedFallback)
{
    outUsedFallback = false;
    if (!pluginContext) return 0;

    uintptr_t address = radarEntity.ComponentCache.BuffsAddr;
    if (address != 0) return address;

    // Correctness fallback: some snapshots may have BuffsAddr=0 even when buffs exist.
    // This is required for rules that depend on `hidden_monster` exclusion.
    address = TryGetBuffsComponentAddressFromDebugList(pluginContext, static_cast<uint32_t>(radarEntity.Id));
    if (address != 0) outUsedFallback = true;

    return address;
}

void AppendBuffsDebugDumpLine(
    const PluginSDK::RadarEntity& radarEntity,
    const PluginSDK::PluginBuffsData* buffsDataOrNull,
    const char* tag)
{
#if !AUTOREFLEX_BUFFS_DUMP
    (void)radarEntity;
    (void)buffsDataOrNull;
    (void)tag;
#else
    std::string outputFilePath;
    bool isEnabled = false;
    {
        std::lock_guard<std::mutex> lock(g_buffsDumpMu);
        outputFilePath = g_buffsDumpPath;
        isEnabled = g_buffsDumpEnabled;
    }
    if (!isEnabled || outputFilePath.empty()) return;

    static std::mutex s_mu;
    static std::unordered_set<uint32_t> s_loggedOnce;

    const uint32_t entityId = static_cast<uint32_t>(radarEntity.Id);
    const bool shouldAlwaysLog = GetIsTrueMatchTag(tag);

    {
        std::lock_guard<std::mutex> lock(s_mu);
        if (!shouldAlwaysLog) {
            if (s_loggedOnce.find(entityId) != s_loggedOnce.end()) return;
            s_loggedOnce.insert(entityId);
        }
    }

    try {
        std::filesystem::path outputPath(outputFilePath);
        std::filesystem::create_directories(outputPath.parent_path());
        std::ofstream file(outputFilePath, std::ios::app);
        if (!file.is_open()) return;

        file << "["
             << (tag ? tag : "TagMissing")
             << "] Id=" << static_cast<uint32_t>(radarEntity.Id)
             << " Path=\"" << AutoReflex::Game::ConvertWideStringUtf16ToUtf8String(radarEntity.Path) << "\"";

        if (!buffsDataOrNull || !buffsDataOrNull->Valid) {
            file << " Buffs=(none)\n";
            return;
        }

        file << " Buffs=[";
        for (size_t buffIndex = 0; buffIndex < buffsDataOrNull->Buffs.size(); ++buffIndex) {
            const auto& buff = buffsDataOrNull->Buffs[buffIndex];
            if (buffIndex) file << ", ";
            file << "{\"Name\":\"" << buff.Name << "\",\"Value\":" << buff.Value << "}";
        }
        file << "]\n";
    } catch (...) {
    }
#endif
}

} // namespace AutoReflex::Scripting::Internal

