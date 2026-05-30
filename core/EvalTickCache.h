// AutoReflex - EvalTickCache
// Per-evaluation-tick caches for mouse position, WorldToScreen, and buff reads.

#pragma once

#include "../sdk/PluginSDK.h"

#include <imgui.h>

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace AutoReflex {
namespace Core {

class EvalTickCache {
public:
    void BeginTick(
        const PluginSDK::Context& pluginContext,
        const PluginSDK::Snapshot& gameSnapshot)
    {
        mouseReady_ = false;
        playerBuffsReady_ = false;
        playerBuffs_.clear();
        cursorByEntityId_.clear();
        buffsByComponentAddr_.clear();

        const ImVec2 mouse = ImGui::GetMousePos();
        mouseX_ = mouse.x;
        mouseY_ = mouse.y;
        mouseReady_ = true;

        if (gameSnapshot.Player.Components.HasBuffs()) {
            playerBuffs_ = pluginContext.Components.EnumerateBuffs(
                gameSnapshot.Player.Components.Buffs);
            playerBuffsReady_ = true;
        }

        playerVitals_ = gameSnapshot.Vitals;
        playerVitalsReady_ = gameSnapshot.Vitals.IsValid;
    }

    int PlayerHPPercent() const { return playerVitalsReady_ ? playerVitals_.HPPercent : 100; }
    int PlayerESPercent() const { return playerVitalsReady_ ? playerVitals_.ESPercent : 100; }
    int PlayerMPPercent() const { return playerVitalsReady_ ? playerVitals_.MPPercent : 100; }
    int PlayerCurrentHP() const { return playerVitalsReady_ ? playerVitals_.CurrentHP : 0; }
    int PlayerMaxHP() const { return playerVitalsReady_ ? playerVitals_.MaxHP : 0; }
    int PlayerCurrentES() const { return playerVitalsReady_ ? playerVitals_.CurrentES : 0; }
    int PlayerMaxES() const { return playerVitalsReady_ ? playerVitals_.MaxES : 0; }
    int PlayerCurrentMP() const { return playerVitalsReady_ ? playerVitals_.CurrentMP : 0; }
    int PlayerMaxMP() const { return playerVitalsReady_ ? playerVitals_.MaxMP : 0; }

    bool PlayerHasGracePeriod() const {
        if (!playerBuffsReady_) return false;
        for (const auto& buff : playerBuffs_) {
            if (buff.Name == "grace_period") return true;
        }
        return false;
    }

    bool GetOrComputeCursorDistance(
        const PluginSDK::Context& pluginContext,
        const PluginSDK::Entity& entity,
        bool needPx,
        double& outDistSq,
        double& outDistPx) const
    {
        const uint32_t entityId = entity.Id;
        auto it = cursorByEntityId_.find(entityId);
        if (it != cursorByEntityId_.end() && it->second.ready) {
            outDistSq = it->second.distSq;
            outDistPx = it->second.distPx;
            return it->second.w2sOk;
        }

        CursorDistCacheEntry entry{};
        entry.ready = true;

        float screenX = 0.f;
        float screenY = 0.f;
        if (mouseReady_ &&
            pluginContext.Render.WorldToScreen(
                entity.WorldX, entity.WorldY, entity.WorldZ, screenX, screenY)) {
            const float deltaX = screenX - mouseX_;
            const float deltaY = screenY - mouseY_;
            entry.distSq = static_cast<double>(deltaX * deltaX + deltaY * deltaY);
            entry.distPx = needPx ? std::sqrt(entry.distSq) : 0.0;
            entry.w2sOk = true;
        } else {
            entry.distSq = 1.0e18;
            entry.distPx = 1.0e9;
            entry.w2sOk = false;
        }

        outDistSq = entry.distSq;
        outDistPx = entry.distPx;
        cursorByEntityId_[entityId] = entry;
        return entry.w2sOk;
    }

    const std::vector<PluginSDK::Buff>* GetOrLoadBuffs(
        const PluginSDK::Context& pluginContext,
        uintptr_t buffsComponentAddress) const
    {
        if (buffsComponentAddress == 0) return nullptr;

        const auto existing = buffsByComponentAddr_.find(buffsComponentAddress);
        if (existing != buffsByComponentAddr_.end()) {
            return &existing->second;
        }

        auto inserted = buffsByComponentAddr_.emplace(
            buffsComponentAddress,
            pluginContext.Components.EnumerateBuffs(buffsComponentAddress));
        return &inserted.first->second;
    }

private:
    struct CursorDistCacheEntry {
        double distSq = 1.0e18;
        double distPx = 1.0e9;
        bool   w2sOk = false;
        bool   ready = false;
    };

    bool mouseReady_ = false;
    float mouseX_ = 0.f;
    float mouseY_ = 0.f;

    bool playerBuffsReady_ = false;
    std::vector<PluginSDK::Buff> playerBuffs_;

    bool playerVitalsReady_ = false;
    PluginSDK::Vitals playerVitals_{};

    mutable std::unordered_map<uint32_t, CursorDistCacheEntry> cursorByEntityId_;
    mutable std::unordered_map<uintptr_t, std::vector<PluginSDK::Buff>> buffsByComponentAddr_;
};

inline thread_local const EvalTickCache* g_ActiveEvalTickCache = nullptr;

class EvalTickCacheScope {
public:
    explicit EvalTickCacheScope(const EvalTickCache& cache) { g_ActiveEvalTickCache = &cache; }
    ~EvalTickCacheScope() { g_ActiveEvalTickCache = nullptr; }

    EvalTickCacheScope(const EvalTickCacheScope&) = delete;
    EvalTickCacheScope& operator=(const EvalTickCacheScope&) = delete;
};

} // namespace Core
} // namespace AutoReflex
