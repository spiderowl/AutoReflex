// ConditionState.cpp
#include "ConditionState.h"

namespace AutoReflex {
namespace Game {

void ConditionState::Update(PluginContext* ctx,
                            const PluginSDK::PluginGameSnapshot* snapshot,
                            Vector2f cursorGridPos)
{
    cursorGridPos_ = cursorGridPos;
    if (snapshot) {
        cache_.Rebuild(ctx, *snapshot, cursorGridPos);
    }
}

void ConditionState::ResetOnAreaChange()
{
    cache_.Clear();
}

int ConditionState::CountMonstersWithBuff(const std::string& buffName) const
{
    return CachedMonsters(cache_).HasBuff(buffName).Count();
}

} // namespace Game
} // namespace AutoReflex