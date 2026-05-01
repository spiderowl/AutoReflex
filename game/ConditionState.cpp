// ConditionState.cpp - stub implementation
#include "ConditionState.h"

namespace AutoReflex {
namespace Game {

void ConditionState::Update(PluginContext* /*ctx*/, const PluginSDK::PluginGameSnapshot* /*snapshot*/) {
    // Phase 1: stub
}

void ConditionState::ResetOnAreaChange() {
    m_Monsters.clear();
}

int ConditionState::CountMonstersWithBuff(const std::string& /*buffName*/) const {
    return 0;
}

} // namespace Game
} // namespace AutoReflex