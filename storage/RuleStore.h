// AutoReflex - RuleStore
// Persists individual rules to rules/<name>.json

#pragma once

#include "../rules/Rule.h"
#include <string>
#include <vector>

namespace AutoReflex {
namespace Storage {

class RuleStore {
public:
    explicit RuleStore(const std::string& rulesDir);

    // Save a single rule to disk
    bool SaveRule(const Rules::Rule& rule);

    // Load a single rule from disk
    bool LoadRule(const std::string& name, Rules::Rule& outRule) const;

    // Delete a rule from disk
    bool DeleteRule(const std::string& name);

    // Rename a rule (move JSON file)
    bool RenameRule(const std::string& oldName, const std::string& newName);

    // List all rule names on disk
    std::vector<std::string> ListRuleNames() const;

    // Load all rules into a vector
    void LoadAll(std::vector<Rules::Rule>& outRules) const;

private:
    std::string m_RulesDir;

    // JSON helpers
    static std::string ruleFilePath(const std::string& rulesDir, const std::string& name);
    static std::string escapeJson(const std::string& s);
};

} // namespace Storage
} // namespace AutoReflex