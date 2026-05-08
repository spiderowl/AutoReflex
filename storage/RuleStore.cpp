// AutoReflex - RuleStore.cpp
// Persists rules to rules/<name>.json

#include "RuleStore.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace AutoReflex { namespace Storage {

// ── JSON string helpers ─────────────────────────────────────────────

std::string RuleStore::escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 20);
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

std::string RuleStore::ruleFilePath(const std::string& rulesDir, const std::string& name) {
    return rulesDir + "/" + name + ".json";
}

// ── Parse helpers ───────────────────────────────────────────────────

static std::string parseTrim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string parseUnescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case '\"': out += '\"'; ++i; break;
                case '\\': out += '\\'; ++i; break;
                case 'n':  out += '\n'; ++i; break;
                case 'r':  out += '\r'; ++i; break;
                case 't':  out += '\t'; ++i; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

static void parseJsonValue(const std::string& json, const std::string& key, std::string& out) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return;

    pos = json.find(':', pos + searchKey.size());
    if (pos == std::string::npos) return;
    ++pos;

    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) ++pos;
    if (pos >= json.size()) return;

    if (json[pos] == '\"') {
        // String value
        size_t start = ++pos;
        size_t end = pos;
        while (end < json.size()) {
            if (json[end] == '\\' && end + 1 < json.size()) { end += 2; continue; }
            if (json[end] == '\"') break;
            ++end;
        }
        out = parseUnescape(json.substr(start, end - start));
    } else {
        // Bool, number, etc.
        size_t end = pos;
        while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != '\n') ++end;
        std::string val = parseTrim(json.substr(pos, end - pos));
        out = val;
    }
}

// ── RuleStore implementation ────────────────────────────────────────

RuleStore::RuleStore(const std::string& rulesDirectoryPath)
    : m_RulesDir(rulesDirectoryPath)
{
    // Ensure directory exists
    fs::create_directories(rulesDirectoryPath);
}

bool RuleStore::SaveRule(const Rules::Rule& rule) {
    std::string path = ruleFilePath(m_RulesDir, rule.Name);

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"Name\": \"" << escapeJson(rule.Name) << "\",\n";
    file << "  \"Enabled\": " << (rule.Enabled ? "true" : "false") << ",\n";
    file << "  \"Key\": " << static_cast<int>(rule.Key) << ",\n";
    file << "  \"CooldownSec\": " << rule.CooldownSec << ",\n";
    file << "  \"WaitAfterPressMs\": " << rule.WaitAfterPressMs << ",\n";
    file << "  \"Order\": " << rule.Order << ",\n";
    file << "  \"ScriptBody\": \"" << escapeJson(rule.ScriptBody) << "\",\n";
    file << "  \"CompileError\": \"" << escapeJson(rule.CompileError) << "\"\n";
    file << "}\n";

    file.close();
    return file.is_open() || fs::exists(path);
}

bool RuleStore::LoadRuleFromDiskByName(const std::string& ruleName, Rules::Rule& outRule) const {
    std::string path = ruleFilePath(m_RulesDir, ruleName);

    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    // Parse fields
    std::string nameVal, enabledVal, keyVal, cdVal, orderVal, scriptVal, errorVal;

    parseJsonValue(content, "Name", nameVal);
    parseJsonValue(content, "Enabled", enabledVal);
    parseJsonValue(content, "Key", keyVal);
    parseJsonValue(content, "CooldownSec", cdVal);
    parseJsonValue(content, "Order", orderVal);
    parseJsonValue(content, "ScriptBody", scriptVal);
    parseJsonValue(content, "CompileError", errorVal);

    outRule.Name = nameVal.empty() ? ruleName : nameVal;
    outRule.Enabled = (enabledVal == "true");
    if (keyVal.empty()) {
        outRule.Key = 0;
    } else {
        int k = 0;
        try { k = std::stoi(keyVal); } catch (...) { k = 0; }
        if (k < 0) k = 0;
        if (k > 0xFFFF) k = 0xFFFF;
        outRule.Key = static_cast<uint16_t>(k);
    }
    outRule.CooldownSec = cdVal.empty() ? 0.5f : std::stof(cdVal);
    std::string waitMsVal;
    parseJsonValue(content, "WaitAfterPressMs", waitMsVal);
    outRule.WaitAfterPressMs = waitMsVal.empty() ? 50.0f : std::stof(waitMsVal);
    outRule.Order = orderVal.empty() ? 0 : std::stoi(orderVal);
    outRule.ScriptBody = scriptVal;
    outRule.CompileError = errorVal;
    outRule.CompiledExpr.reset();
    outRule.LastEvalResult = false;
    outRule.EverFired = false;
    outRule.LastFired = std::chrono::steady_clock::time_point();

    return true;
}

bool RuleStore::DeleteRuleFromDiskByName(const std::string& ruleName) {
    std::string path = ruleFilePath(m_RulesDir, ruleName);
    return fs::remove(path) != 0;
}

bool RuleStore::RenameRuleOnDisk(const std::string& oldRuleName, const std::string& newRuleName) {
    std::string oldPath = ruleFilePath(m_RulesDir, oldRuleName);
    std::string newPath = ruleFilePath(m_RulesDir, newRuleName);

    if (!fs::exists(oldPath)) return false;
    if (fs::exists(newPath)) fs::remove(newPath);

    try {
        fs::rename(oldPath, newPath);
        return fs::exists(newPath);
    } catch (...) {
        return false;
    }
}

std::vector<std::string> RuleStore::ListRuleNames() const {
    std::vector<std::string> names;

    if (!fs::exists(m_RulesDir) || !fs::is_directory(m_RulesDir)) return names;

    for (const auto& entry : fs::directory_iterator(m_RulesDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::string stem = entry.path().stem().string();
            names.push_back(stem);
        }
    }

    std::sort(names.begin(), names.end());
    return names;
}

void RuleStore::LoadAllRulesFromDisk(std::vector<Rules::Rule>& outRules) const {
    outRules.clear();

    auto names = ListRuleNames();

    for (const auto& name : names) {
        Rules::Rule rule;
        if (LoadRuleFromDiskByName(name, rule)) {
            outRules.push_back(std::move(rule));
        }
    }

    // Sort by order
    std::sort(outRules.begin(), outRules.end(), [](const Rules::Rule& a, const Rules::Rule& b) {
        return a.Order < b.Order;
    });
}

} } // namespace AutoReflex::Storage