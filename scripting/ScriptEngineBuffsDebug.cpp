// AutoReflex - ScriptEngineBuffsDebug.cpp

#include "ScriptEngineBuffsDebug.h"

#include "../game/MonsterHelpers.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_set>

#if defined(_DEBUG) || defined(AUTOREFLEX_ENABLE_BUFFS_DUMP)
#define AUTOREFLEX_BUFFS_DUMP 1
#else
#define AUTOREFLEX_BUFFS_DUMP 0
#endif

namespace AutoReflex::Scripting::Internal {

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

uintptr_t ResolveBuffsComponentAddress(const PluginSDK::Entity& entity)
{
    return entity.Components.Buffs;
}

void AppendBuffsDebugDumpLine(
    const PluginSDK::Entity& entity,
    const std::vector<PluginSDK::Buff>* buffsOrNull,
    const char* tag)
{
#if !AUTOREFLEX_BUFFS_DUMP
    (void)entity;
    (void)buffsOrNull;
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

    const uint32_t entityId = entity.Id;
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
             << "] Id=" << entityId
             << " Path=\"" << AutoReflex::Game::ConvertWideStringUtf16ToUtf8String(entity.Path) << "\"";

        if (!buffsOrNull || buffsOrNull->empty()) {
            file << " Buffs=(none)\n";
            return;
        }

        file << " Buffs=[";
        for (size_t buffIndex = 0; buffIndex < buffsOrNull->size(); ++buffIndex) {
            const auto& buff = (*buffsOrNull)[buffIndex];
            if (buffIndex) file << ", ";
            file << "{\"Name\":\"" << buff.Name << "\",\"Charges\":" << buff.Charges << "}";
        }
        file << "]\n";
    } catch (...) {
    }
#endif
}

} // namespace AutoReflex::Scripting::Internal
