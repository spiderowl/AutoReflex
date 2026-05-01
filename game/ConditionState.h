// AutoReflex - ConditionState
// Tracks game state for rule evaluation (Phase 2+)

#pragma once

#include "MonsterInfo.h"
#include <vector>

struct PluginContext;
namespace PluginSDK { struct PluginGameSnapshot; }

namespace AutoReflex {
namespace Game {

class ConditionState {
public:
    void Update(PluginContext* ctx, const PluginSDK::PluginGameSnapshot* snapshot);
    void ResetOnAreaChange();
    
    size_t GetMonsterCount() const { return m_Monsters.size(); }
    const std::vector<MonsterData>& GetMonsters() const { return m_Monsters; }
    int CountMonstersWithBuff(const std::string& buffName) const;

private:
    std::vector<MonsterData> m_Monsters;
};

} // namespace Game
} // namespace AutoReflex