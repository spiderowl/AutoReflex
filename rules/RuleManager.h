// AutoReflex - RuleManager
// Manages collection of rules and evaluates them

#pragma once
#include "Rule.h"
#include <vector>
#include <functional>
#include <string>
#include <memory>

struct PluginContext;
namespace AutoReflex { namespace Game { class ConditionState; }}
namespace AutoReflex { namespace Storage { class RuleStore; }}

namespace AutoReflex {
namespace Rules {

class RuleManager {
public:
    explicit RuleManager(const std::string& dataDir);
    
    void LoadRules(Storage::RuleStore& store);
    
    void EvaluateAll(
        PluginContext* ctx,
        Game::ConditionState& conditionState,
        std::function<void(const Rule&)> onFire);
    
    std::vector<Rule>& GetRules() { return m_Rules; }
    const std::vector<Rule>& GetRules() const { return m_Rules; }

private:
    std::string m_DataDir;
    std::vector<Rule> m_Rules;
};

} // namespace Rules
} // namespace AutoReflex