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
    explicit RuleStore(const std::string& rulesDirectoryPath);

    /** Saves a single rule to disk. */
    bool SaveRule(const Rules::Rule& rule);

    /**
     * Loads a single rule from disk.
     *
     * @param ruleName File stem under the rules directory.
     * @param outRule Output rule populated on success.
     * @returns True on success; otherwise false.
     */
    bool LoadRuleFromDiskByName(const std::string& ruleName, Rules::Rule& outRule) const;

    /** Deletes a rule file from disk. */
    bool DeleteRuleFromDiskByName(const std::string& ruleName);

    /** Renames a rule by moving its JSON file on disk. */
    bool RenameRuleOnDisk(const std::string& oldRuleName, const std::string& newRuleName);

    /** Lists all rule names currently present on disk. */
    std::vector<std::string> ListRuleNames() const;

    /**
     * Loads all rules into a vector.
     *
     * @param outRules Output vector populated and sorted by `Order`.
     * @returns None.
     */
    void LoadAllRulesFromDisk(std::vector<Rules::Rule>& outRules) const;

private:
    std::string m_RulesDir;

    // JSON helpers
    static std::string ruleFilePath(const std::string& rulesDir, const std::string& name);
    static std::string escapeJson(const std::string& s);
};

} // namespace Storage
} // namespace AutoReflex