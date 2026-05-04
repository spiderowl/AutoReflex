// AutoReflex - ScriptEngine.cpp
// EXPRTK-based expression evaluation engine
// Header-only library — no external linking required

#include "ScriptEngine.h"
#include "../core/AgentDebugLog.h"
#include "../sdk/PluginContext.h"
#include "../game/MonsterHelpers.h"

#include "exprtk.hpp"

#include <imgui.h>

#include <sstream>
#include <cassert>
#include <iostream>
#include <vector>
#include <string>

// ============================================================================
// CompiledExpression implementation
// ============================================================================

typedef exprtk::symbol_table<double> symbol_table_t;
typedef exprtk::expression<double>   expression_t;
typedef exprtk::parser<double>       parser_t;

CompiledExpression::CompiledExpression()
    : expression_(std::make_unique<expression_t>())
    , symbolTable_(std::make_unique<symbol_table_t>())
    , parser_(std::make_unique<parser_t>())
{
    // Register entity field variables with the symbol table
    symbolTable_->add_variable("e_Id", e_Id);
    symbolTable_->add_variable("e_IsValid", e_IsValid);
    symbolTable_->add_variable("e_Rarity", e_Rarity);
    symbolTable_->add_variable("e_Zone", e_Zone);
    symbolTable_->add_variable("e_GridPositionX", e_GridPositionX);
    symbolTable_->add_variable("e_GridPositionY", e_GridPositionY);
    symbolTable_->add_variable("e_GridPositionZ", e_GridPositionZ);
    symbolTable_->add_variable("e_WorldX", e_WorldX);
    symbolTable_->add_variable("e_WorldY", e_WorldY);
    symbolTable_->add_variable("e_WorldZ", e_WorldZ);
    symbolTable_->add_variable("e_CurrentHP", e_CurrentHP);
    symbolTable_->add_variable("e_MaxHP", e_MaxHP);
    symbolTable_->add_variable("e_CurrentES", e_CurrentES);
    symbolTable_->add_variable("e_MaxES", e_MaxES);
    symbolTable_->add_variable("e_IsSleeping", e_IsSleeping);
    symbolTable_->add_variable("e_CursorDistPx", e_CursorDistPx);
    symbolTable_->add_variable("e_Reaction", e_Reaction);
    symbolTable_->add_variable("e_IsTargetable", e_IsTargetable);

    // Register context variables (cursor position in grid coordinates)
    curX = 0.0;
    curY = 0.0;
    symbolTable_->add_variable("curX", curX);
    symbolTable_->add_variable("curY", curY);

    // Register mathematical constants
    symbolTable_->add_constants();

    // Register the symbol table with the expression
    expression_->register_symbol_table(*symbolTable_);
}

CompiledExpression::~CompiledExpression() = default;

namespace {
    // Very small, purpose-built preprocessor:
    // - hasBuff("poison")      -> hasBuffIdx(0)
    // - hasBuffValue("poison") -> hasBuffValueIdx(0)
    // - pathContains("skeleton")-> pathContainsIdx(0)
    //
    // It records each unique string needle into output vectors and passes the index
    // as a numeric argument to EXPRTK functions.
    static bool ExtractQuotedArg(const std::string& s, size_t startQuote, std::string& out, size_t& outNextPos) {
        if (startQuote >= s.size() || s[startQuote] != '"') return false;
        size_t i = startQuote + 1;
        std::string buf;
        buf.reserve(32);
        while (i < s.size()) {
            char c = s[i];
            if (c == '\\' && i + 1 < s.size()) {
                // minimal escape support for \" and \\.
                char n = s[i + 1];
                if (n == '"' || n == '\\') { buf.push_back(n); i += 2; continue; }
            }
            if (c == '"') { out = buf; outNextPos = i + 1; return true; }
            buf.push_back(c);
            ++i;
        }
        return false;
    }

    static int InternNeedle(std::vector<std::string>& v, const std::string& needle) {
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i] == needle) return static_cast<int>(i);
        }
        v.push_back(needle);
        return static_cast<int>(v.size() - 1);
    }

    static std::string ToLowerAscii(std::string s) {
        for (char& c : s) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        }
        return s;
    }

    static void SkipWs(const std::string& s, size_t& i) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
    }

    static std::string TrimAscii(const std::string& s) {
        size_t a = 0;
        while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
        size_t b = s.size();
        while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
        return s.substr(a, b - a);
    }

    static std::vector<std::string> SplitByPipe(const std::string& s) {
        std::vector<std::string> out;
        size_t i = 0;
        while (i < s.size()) {
            size_t j = i;
            while (j < s.size() && s[j] != '|') ++j;
            out.push_back(TrimAscii(s.substr(i, j - i)));
            i = (j < s.size()) ? (j + 1) : j;
        }
        // remove empties
        out.erase(std::remove_if(out.begin(), out.end(), [](const std::string& x) { return x.empty(); }), out.end());
        return out;
    }

    // Case-sensitive tokens for end-user DSL.
    static bool RarityTokenToValue(const std::string& token, int& outValue, bool& outAtLeast) {
        outAtLeast = false;
        if (token == "any") { outValue = 0; return true; }
        if (token == "normal") { outValue = 0; return true; }
        if (token == "magic") { outValue = 1; return true; }
        if (token == "rare") { outValue = 2; return true; }
        if (token == "unique") { outValue = 3; return true; }
        if (token == "atleastmagic") { outValue = 1; outAtLeast = true; return true; }
        if (token == "atleastrare") { outValue = 2; outAtLeast = true; return true; }
        if (token == "atleastunique") { outValue = 3; outAtLeast = true; return true; }
        return false;
    }

    static bool ParseIdentifier(const std::string& s, size_t& i, std::string& outIdent) {
        size_t start = i;
        while (i < s.size()) {
            char c = s[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
                ++i;
            } else {
                break;
            }
        }
        if (i == start) return false;
        outIdent.assign(s.begin() + start, s.begin() + i);
        return true;
    }

    static bool ParseParenArg(const std::string& s, size_t& i, std::string& outArg, std::string& errorMsg) {
        SkipWs(s, i);
        if (i >= s.size() || s[i] != '(') return false;
        ++i;
        SkipWs(s, i);
        if (i >= s.size()) { errorMsg = "Missing ')'"; return false; }
        if (s[i] == ')') {
            // Allow empty args for no-arg methods like hostile().
            outArg.clear();
            ++i;
            return true;
        }

        // Quoted string?
        if (s[i] == '"') {
            size_t nextPos = i;
            std::string needle;
            if (!ExtractQuotedArg(s, i, needle, nextPos)) { errorMsg = "String is not closed"; return false; }
            outArg = needle;
            i = nextPos;
        } else {
            // Bare token until ')'
            size_t start = i;
            while (i < s.size() && s[i] != ')') ++i;
            if (i >= s.size()) { errorMsg = "Missing ')'"; return false; }
            outArg = TrimAscii(s.substr(start, i - start));
        }

        SkipWs(s, i);
        if (i >= s.size() || s[i] != ')') { errorMsg = "Missing ')'"; return false; }
        ++i;
        return true;
    }

    static bool ParseParenArgs(const std::string& s, size_t& i, std::vector<std::string>& outArgs, std::string& errorMsg) {
        outArgs.clear();
        SkipWs(s, i);
        if (i >= s.size() || s[i] != '(') return false;
        ++i;

        while (i < s.size()) {
            SkipWs(s, i);
            if (i >= s.size()) { errorMsg = "Missing ')'"; return false; }
            if (s[i] == ')') { ++i; break; }

            // Parse one arg (quoted string or bare token).
            std::string arg;
            if (s[i] == '"') {
                size_t nextPos = i;
                std::string needle;
                if (!ExtractQuotedArg(s, i, needle, nextPos)) { errorMsg = "String is not closed"; return false; }
                arg = needle;
                i = nextPos;
            } else {
                size_t start = i;
                while (i < s.size() && s[i] != ',' && s[i] != ')') ++i;
                if (i > start) arg = TrimAscii(s.substr(start, i - start));
                else { errorMsg = "Missing argument"; return false; }
            }

            outArgs.push_back(arg);
            SkipWs(s, i);
            if (i >= s.size()) { errorMsg = "Missing ')'"; return false; }
            if (s[i] == ',') { ++i; continue; }
            if (s[i] == ')') { ++i; break; }
            errorMsg = "Expected ',' or ')'";
            return false;
        }

        return true;
    }

    // Translate a DSL root + fluent chain into a boolean-ish expr (0/1).
    // Roots implemented:
    //   hostileMinionCount  -> hostile + alive (+ targetable default)
    //   friendlyMinionCount -> friendly + alive (+ targetable default)
    //   corpseCount         -> hostile + dead (no targetable default)
    static bool TranslateCountChain(const std::string& s,
                                    size_t& i,
                                    const char* root,
                                    size_t rootLen,
                                    int forcedReaction,     // -1 = no force, else 0/2
                                    int forcedAliveState,   // -1 = no force, 1 = alive, 0 = dead
                                    bool defaultTargetable, // add targetable() by default
                                    const char* unknownMethodLabel,
                                    std::string& outExpr,
                                    std::vector<std::string>& buffNeedles,
                                    std::vector<std::string>& pathNeedles,
                                    std::string& errorMsg)
    {
        if (s.compare(i, rootLen, root) != 0) return false;
        size_t p = i + rootLen;

        std::vector<std::string> conds;
        bool sawReaction = (forcedReaction != -1);
        bool sawAlive = (forcedAliveState != -1);
        bool sawTargetable = false;

        if (forcedReaction != -1) {
            conds.push_back("(e_Reaction==" + std::to_string(forcedReaction) + ")");
        }
        if (forcedAliveState == 1) {
            conds.push_back("(e_CurrentHP>0)");
        } else if (forcedAliveState == 0) {
            conds.push_back("(e_CurrentHP<=0)");
        }

        while (p < s.size()) {
            SkipWs(s, p);
            if (p >= s.size() || s[p] != '.') break;
            ++p;
            SkipWs(s, p);

            std::string method;
            if (!ParseIdentifier(s, p, method)) { errorMsg = "Expected method after '.'"; return false; }

            std::string arg;
            std::vector<std::string> args;
            if (method == "hasBuffValue") {
                if (!ParseParenArgs(s, p, args, errorMsg)) { errorMsg = "Expected '(...)' after method"; return false; }
            } else {
                if (!ParseParenArg(s, p, arg, errorMsg)) { errorMsg = "Expected '(...)' after method"; return false; }
            }

            if (method == "zone") {
                int z = 0;
                if (arg == "inner") z = 1;
                else if (arg == "outer") z = 2;
                else if (arg == "far") z = 3;
                else { errorMsg = "zone() expects inner/outer/far"; return false; }
                conds.push_back("(e_Zone==" + std::to_string(z) + ")");
            } else if (method == "nearCursor") {
                conds.push_back("(e_CursorDistPx<=" + arg + ")");
            } else if (method == "hasBuff") {
                int idx = InternNeedle(buffNeedles, arg);
                conds.push_back("hasBuffIdx(" + std::to_string(idx) + ")");
            } else if (method == "hasName") {
                int idx = InternNeedle(pathNeedles, arg);
                conds.push_back("pathContainsIdx(" + std::to_string(idx) + ")");
            } else if (method == "hasBuffValue") {
                // hasBuffValue("contagion", 3) -> (hasBuffValueIdx(n)==3)
                if (args.size() != 2) { errorMsg = "hasBuffValue() expects 2 args: \"name\",number"; return false; }
                int idx = InternNeedle(buffNeedles, args[0]);
                conds.push_back("(hasBuffValueIdx(" + std::to_string(idx) + ")==" + args[1] + ")");
            } else if (method == "hostile") {
                // Reaction: 0=Hostile 1=Neutral 2=Friendly
                if (forcedReaction != -1) { errorMsg = std::string(root) + " does not support hostile()/friendly()"; return false; }
                sawReaction = true;
                conds.push_back("(e_Reaction==0)");
            } else if (method == "friendly") {
                if (forcedReaction != -1) { errorMsg = std::string(root) + " does not support hostile()/friendly()"; return false; }
                sawReaction = true;
                conds.push_back("(e_Reaction==2)");
            } else if (method == "alive") {
                if (forcedAliveState != -1) { errorMsg = std::string(root) + " does not support alive() here"; return false; }
                sawAlive = true;
                conds.push_back("(e_CurrentHP>0)");
            } else if (method == "dead") {
                if (forcedAliveState != -1) { errorMsg = std::string(root) + " does not support dead() here"; return false; }
                sawAlive = true;
                conds.push_back("(e_CurrentHP<=0)");
            } else if (method == "targetable") {
                sawTargetable = true;
                conds.push_back("(e_IsTargetable==1)");
            } else if (method == "type") {
                // Rarity filters.
                // Examples:
                //   type(any)
                //   type(atleastmagic)
                //   type(magic|rare)
                auto parts = SplitByPipe(arg);
                if (parts.empty()) { errorMsg = "type() expects a rarity token"; return false; }

                // If any part is an atleast* token, treat as >= max(atleast).
                int atleastVal = -1;
                bool sawAtLeast = false;
                std::vector<int> equalsVals;

                for (auto& pTok : parts) {
                    std::string t = pTok;
                    int v = 0; bool atLeast = false;
                    if (!RarityTokenToValue(t, v, atLeast)) { errorMsg = "Unknown type() value: " + pTok; return false; }
                    if (t == "any") {
                        // no-op
                        continue;
                    }
                    if (atLeast) {
                        sawAtLeast = true;
                        if (v > atleastVal) atleastVal = v;
                    } else {
                        equalsVals.push_back(v);
                    }
                }

                if (sawAtLeast) {
                    conds.push_back("(e_Rarity>=" + std::to_string(atleastVal) + ")");
                }
                if (!equalsVals.empty()) {
                    // Build (e_Rarity==a) or (e_Rarity==b) ...
                    std::string expr = "(";
                    for (size_t k = 0; k < equalsVals.size(); ++k) {
                        if (k) expr += " or ";
                        expr += "(e_Rarity==" + std::to_string(equalsVals[k]) + ")";
                    }
                    expr += ")";
                    conds.push_back(expr);
                }
            } else {
                errorMsg = std::string("Unknown ") + unknownMethodLabel + " method: " + method;
                return false;
            }
        }

        // Defaults (keep end-user rules short).
        if (!sawReaction) conds.insert(conds.begin(), "(e_Reaction==0)");
        if (!sawAlive) conds.insert(conds.begin(), "(e_CurrentHP>0)");
        if (defaultTargetable && !sawTargetable) conds.insert(conds.begin(), "(e_IsTargetable==1)");

        if (conds.empty()) outExpr = "1";
        else {
            outExpr = "(";
            for (size_t k = 0; k < conds.size(); ++k) {
                // Use EXPRTK boolean operators.
                if (k) outExpr += " and ";
                outExpr += conds[k];
            }
            outExpr += ")";
        }

        // Note: EXPRTK doesn't support C-style ternary. Our conditions are already
        // numeric (comparisons/functions => 0/1), so callers can do `> 0`.

        i = p;
        return true;
    }

    static bool TranslateHostileMinionCountChain(const std::string& s,
                                                 size_t& i,
                                                 std::string& outExpr,
                                                 std::vector<std::string>& buffNeedles,
                                                 std::vector<std::string>& pathNeedles,
                                                 std::string& errorMsg)
    {
        return TranslateCountChain(
            s, i,
            "hostileMinionCount", (sizeof("hostileMinionCount") - 1),
            0,   // hostile
            1,   // alive
            true,// default targetable
            "hostileMinionCount",
            outExpr, buffNeedles, pathNeedles, errorMsg);
    }

    static bool TranslateFriendlyMinionCountChain(const std::string& s,
                                                  size_t& i,
                                                  std::string& outExpr,
                                                  std::vector<std::string>& buffNeedles,
                                                  std::vector<std::string>& pathNeedles,
                                                  std::string& errorMsg)
    {
        return TranslateCountChain(
            s, i,
            "friendlyMinionCount", (sizeof("friendlyMinionCount") - 1),
            2,   // friendly
            1,   // alive
            true,// default targetable
            "friendlyMinionCount",
            outExpr, buffNeedles, pathNeedles, errorMsg);
    }

    static bool TranslateCorpseCountChain(const std::string& s,
                                          size_t& i,
                                          std::string& outExpr,
                                          std::vector<std::string>& buffNeedles,
                                          std::vector<std::string>& pathNeedles,
                                          std::string& errorMsg)
    {
        return TranslateCountChain(
            s, i,
            "corpseCount", (sizeof("corpseCount") - 1),
            0,   // hostile corpses
            0,   // dead only
            false, // do not default targetable for corpses
            "corpseCount",
            outExpr, buffNeedles, pathNeedles, errorMsg);
    }

    static bool PreprocessExpression(const std::string& in,
                                     std::string& out,
                                     std::vector<std::string>& buffNeedles,
                                     std::vector<std::string>& pathNeedles,
                                     std::string& errorMsg)
    {
        out.clear();
        out.reserve(in.size());

        size_t i = 0;
        while (i < in.size()) {
            // Fluent DSL: monstercount.<chain>
            {
                std::string translated;
                size_t save = i;
                if (TranslateHostileMinionCountChain(in, save, translated, buffNeedles, pathNeedles, errorMsg) ||
                    TranslateFriendlyMinionCountChain(in, save, translated, buffNeedles, pathNeedles, errorMsg) ||
                    TranslateCorpseCountChain(in, save, translated, buffNeedles, pathNeedles, errorMsg)) {
                    out += translated;
                    i = save;
                    continue;
                }
            }

            // Look for hasBuff(
            if (in.compare(i, 8, "hasBuff(") == 0) {
                size_t j = i + 8;
                while (j < in.size() && (in[j] == ' ' || in[j] == '\t')) ++j;
                if (j >= in.size() || in[j] != '"') { errorMsg = "hasBuff() expects a quoted string"; return false; }
                std::string needle;
                size_t nextPos = j;
                if (!ExtractQuotedArg(in, j, needle, nextPos)) { errorMsg = "hasBuff() string is not closed"; return false; }
                while (nextPos < in.size() && (in[nextPos] == ' ' || in[nextPos] == '\t')) ++nextPos;
                if (nextPos >= in.size() || in[nextPos] != ')') { errorMsg = "hasBuff() missing ')'"; return false; }
                int idx = InternNeedle(buffNeedles, needle);
                out += "hasBuffIdx(" + std::to_string(idx) + ")";
                i = nextPos + 1;
                continue;
            }

            // Look for hasBuffValue(
            if (in.compare(i, 12, "hasBuffValue(") == 0) {
                size_t j = i + 12;
                while (j < in.size() && (in[j] == ' ' || in[j] == '\t')) ++j;
                if (j >= in.size() || in[j] != '"') { errorMsg = "hasBuffValue() expects a quoted string"; return false; }
                std::string needle;
                size_t nextPos = j;
                if (!ExtractQuotedArg(in, j, needle, nextPos)) { errorMsg = "hasBuffValue() string is not closed"; return false; }
                while (nextPos < in.size() && (in[nextPos] == ' ' || in[nextPos] == '\t')) ++nextPos;
                if (nextPos >= in.size() || in[nextPos] != ')') { errorMsg = "hasBuffValue() missing ')'"; return false; }
                int idx = InternNeedle(buffNeedles, needle);
                out += "hasBuffValueIdx(" + std::to_string(idx) + ")";
                i = nextPos + 1;
                continue;
            }

            // Look for pathContains(
            if (in.compare(i, 13, "pathContains(") == 0) {
                size_t j = i + 13;
                while (j < in.size() && (in[j] == ' ' || in[j] == '\t')) ++j;
                if (j >= in.size() || in[j] != '"') { errorMsg = "pathContains() expects a quoted string"; return false; }
                std::string needle;
                size_t nextPos = j;
                if (!ExtractQuotedArg(in, j, needle, nextPos)) { errorMsg = "pathContains() string is not closed"; return false; }
                while (nextPos < in.size() && (in[nextPos] == ' ' || in[nextPos] == '\t')) ++nextPos;
                if (nextPos >= in.size() || in[nextPos] != ')') { errorMsg = "pathContains() missing ')'"; return false; }
                int idx = InternNeedle(pathNeedles, needle);
                out += "pathContainsIdx(" + std::to_string(idx) + ")";
                i = nextPos + 1;
                continue;
            }

            out.push_back(in[i]);
            ++i;
        }

        errorMsg.clear();
        return true;
    }

    // EXPRTK integration: simplest supported path is C function pointers.
    // We set the current expression pointer in Evaluate() (thread_local) so
    // hasBuffIdx()/pathContainsIdx() can call back into the right instance.
    thread_local const CompiledExpression* tl_expr = nullptr;

    static double HasBuffIdxThunk(double idx) {
        if (!tl_expr) return 0.0;
        return tl_expr->HasBuffIdx(static_cast<int>(idx)) ? 1.0 : 0.0;
    }

    static double HasBuffValueIdxThunk(double idx) {
        if (!tl_expr) return 0.0;
        return tl_expr->HasBuffValueIdx(static_cast<int>(idx));
    }

    static double PathContainsIdxThunk(double idx) {
        if (!tl_expr) return 0.0;
        return tl_expr->PathContainsIdx(static_cast<int>(idx)) ? 1.0 : 0.0;
    }

    static double Dummy1Thunk(double) { return 0.0; }
}

bool CompiledExpression::Compile(const std::string& exprString, std::string& errorMsg) {
    exprString_ = exprString;
    compiledString_.clear();
    buffNeedles_.clear();
    pathNeedles_.clear();

    // Preprocess to replace string-arg helpers with idx-based functions.
    if (!PreprocessExpression(exprString_, compiledString_, buffNeedles_, pathNeedles_, errorMsg)) {
        // #region agent log
        ArAgentNdjsonLog("H-EXPR", "CompiledExpression::Compile.PreprocessFailed", errorMsg, "{}");
        // #endregion
        return false;
    }

    // Register custom functions (idx-based).
    // These resolve via thread_local set in Evaluate().
    symbolTable_->add_function("hasBuffIdx", &HasBuffIdxThunk);
    symbolTable_->add_function("hasBuffValueIdx", &HasBuffValueIdxThunk);
    symbolTable_->add_function("pathContainsIdx", &PathContainsIdxThunk);
    // (No standalone global count functions; use fluent roots instead.)

    // Attempt to parse the expression
    bool success = parser_->compile(compiledString_, *expression_);

    if (!success) {
        errorMsg = "Expression compilation failed. Translated: " + compiledString_;
        // #region agent log
        ArAgentNdjsonLog("H-EXPR", "CompiledExpression::Compile", "Expression compilation failed: " + compiledString_, "{}");
        // #endregion
        return false;
    }

    errorMsg.clear();
    // #region agent log
    {
        std::ostringstream d;
        d << "{\"exprLen\":" << exprString.size() << "}";
        ArAgentNdjsonLog("H-EXPR", "CompiledExpression::Compile", "Expression compiled successfully", d.str());
    }
    // #endregion
    return true;
}

bool CompiledExpression::Evaluate(PluginContext* ctx,
                                  const PluginSDK::RadarEntity& entity,
                                  double cursorX, double cursorY) const
{
    if (!expression_) return false;

    tl_expr = this;

    // Bind entity fields to variables
    e_Id              = static_cast<double>(entity.Id);
    e_IsValid         = entity.IsValid ? 1.0 : 0.0;
    e_Rarity          = static_cast<double>(entity.Rarity);
    e_Zone            = static_cast<double>(static_cast<int>(entity.Zone));
    e_GridPositionX   = static_cast<double>(entity.GridPositionX);
    e_GridPositionY   = static_cast<double>(entity.GridPositionY);
    e_GridPositionZ   = 0.0; // Not directly available, set to 0
    e_WorldX          = static_cast<double>(entity.WorldX);
    e_WorldY          = static_cast<double>(entity.WorldY);
    e_WorldZ          = static_cast<double>(entity.WorldZ);
    e_CurrentHP       = static_cast<double>(entity.CurrentHP);
    e_MaxHP           = static_cast<double>(entity.MaxHP);
    e_CurrentES       = static_cast<double>(entity.CurrentES);
    e_MaxES           = static_cast<double>(entity.MaxES);
    e_IsSleeping      = entity.IsSleeping ? 1.0 : 0.0;
    e_Reaction        = static_cast<double>(entity.Reaction);
    e_IsTargetable    = 1.0;

    // Set evaluation context for custom functions
    curCtx_ = ctx;
    curEnt_ = &entity;
    if (!buffNeedles_.empty()) {
        if (buffResultCache_.size() != buffNeedles_.size()) buffResultCache_.assign(buffNeedles_.size(), -1);
        else std::fill(buffResultCache_.begin(), buffResultCache_.end(), -1);

        if (buffValueCache_.size() != buffNeedles_.size()) buffValueCache_.assign(buffNeedles_.size(), (int16_t)-32768);
        else std::fill(buffValueCache_.begin(), buffValueCache_.end(), (int16_t)-32768);
    }
    if (!pathNeedles_.empty()) {
        if (pathResultCache_.size() != pathNeedles_.size()) pathResultCache_.assign(pathNeedles_.size(), -1);
        else std::fill(pathResultCache_.begin(), pathResultCache_.end(), -1);
    }

    // Bind context variables
    curX = cursorX;
    curY = cursorY;

    // Cursor distance in screen pixels (AutoAim-style).
    // If projection isn't available, use a very large value so `<= threshold` fails.
    e_CursorDistPx = 1.0e9;
    if (ctx && ctx->WorldToScreen) {
        float sx = 0.f, sy = 0.f;
        if (ctx->WorldToScreen(entity.WorldX, entity.WorldY, entity.WorldZ, &sx, &sy)) {
            ImVec2 mouse = ImGui::GetMousePos();
            float dx = sx - mouse.x;
            float dy = sy - mouse.y;
            e_CursorDistPx = std::sqrt(dx * dx + dy * dy);
        }
    }

    // Targetable (helps avoid immune/untargetable animation phases).
    if (ctx && ctx->ReadTargetableComponent && entity.ComponentCache.HasTargetable()) {
        auto t = ctx->ReadTargetableComponent(entity.ComponentCache.TargetableAddr);
        e_IsTargetable = (t.Valid && t.IsTargetable) ? 1.0 : 0.0;
    }

    // Evaluate — result is in expression_->value()
    double result = expression_->value();

    // Treat nonzero as true (boolean semantics)
    return result != 0.0;
}

bool CompiledExpression::HasBuffIdx(int idx) const
{
    if (!curCtx_ || !curEnt_) return false;
    if (idx < 0 || idx >= static_cast<int>(buffNeedles_.size())) return false;
    if (!curEnt_->ComponentCache.HasBuffs()) return false;
    if (!curCtx_->ReadBuffsComponent) return false;

    if (idx < static_cast<int>(buffResultCache_.size()) && buffResultCache_[idx] != -1)
        return buffResultCache_[idx] == 1;

    const auto data = curCtx_->ReadBuffsComponent(curEnt_->ComponentCache.BuffsAddr);
    if (!data.Valid) {
        if (idx < static_cast<int>(buffResultCache_.size())) buffResultCache_[idx] = 0;
        return false;
    }

    bool found = false;
    const auto& needle = buffNeedles_[idx];
    for (const auto& b : data.Buffs) {
        if (b.Name == needle) { found = true; break; }
    }
    if (idx < static_cast<int>(buffResultCache_.size())) buffResultCache_[idx] = found ? 1 : 0;
    return found;
}

double CompiledExpression::HasBuffValueIdx(int idx) const
{
    if (!curCtx_ || !curEnt_) return 0.0;
    if (idx < 0 || idx >= static_cast<int>(buffNeedles_.size())) return 0.0;
    if (!curEnt_->ComponentCache.HasBuffs()) return 0.0;
    if (!curCtx_->ReadBuffsComponent) return 0.0;

    if (idx < static_cast<int>(buffValueCache_.size()) && buffValueCache_[idx] != (int16_t)-32768)
        return static_cast<double>(buffValueCache_[idx]);

    const auto data = curCtx_->ReadBuffsComponent(curEnt_->ComponentCache.BuffsAddr);
    if (!data.Valid) {
        if (idx < static_cast<int>(buffValueCache_.size())) buffValueCache_[idx] = 0;
        return 0.0;
    }

    int16_t value = 0;
    const auto& needle = buffNeedles_[idx];
    for (const auto& b : data.Buffs) {
        if (b.Name == needle) { value = b.Charges; break; }
    }

    if (idx < static_cast<int>(buffValueCache_.size())) buffValueCache_[idx] = value;
    return static_cast<double>(value);
}

bool CompiledExpression::PathContainsIdx(int idx) const
{
    if (!curEnt_) return false;
    if (idx < 0 || idx >= static_cast<int>(pathNeedles_.size())) return false;

    if (idx < static_cast<int>(pathResultCache_.size()) && pathResultCache_[idx] != -1)
        return pathResultCache_[idx] == 1;

    const std::string path = AutoReflex::Game::WStringToString(curEnt_->Path);
    const bool ok = AutoReflex::Game::ContainsIgnoreCase(path, pathNeedles_[idx]);
    if (idx < static_cast<int>(pathResultCache_.size())) pathResultCache_[idx] = ok ? 1 : 0;
    return ok;
}

// ============================================================================
// ScriptEngine implementation
// ============================================================================

ScriptEngine::ScriptEngine() {}

ScriptEngine::~ScriptEngine() {}

bool ScriptEngine::Initialize() {
    lastError.clear();
    initialized_ = true;

    // #region agent log
    ArAgentNdjsonLog("H-INIT", "ScriptEngine::Initialize", "EXPRTK engine initialized (header-only)", "{}");
    // #endregion

    return true;
}

static bool IsTargetableNow(PluginContext* ctx, const PluginSDK::RadarEntity& e) {
    if (!ctx || !ctx->ReadTargetableComponent) return true; // if we can't read it, don't block
    if (!e.ComponentCache.HasTargetable()) return true;
    auto t = ctx->ReadTargetableComponent(e.ComponentCache.TargetableAddr);
    return t.Valid && t.IsTargetable;
}

bool ScriptEngine::ValidateExpression(const std::string& expr, std::string& errorMsg) {
    // Quick compile-test without keeping the compiled result
    symbol_table_t symbolTable;
    expression_t expression;
    parser_t parser;

    // Register the same variables
    double dummy = 0.0;
    symbolTable.add_variable("e_Id", dummy);
    symbolTable.add_variable("e_IsValid", dummy);
    symbolTable.add_variable("e_Rarity", dummy);
    symbolTable.add_variable("e_Zone", dummy);
    symbolTable.add_variable("e_GridPositionX", dummy);
    symbolTable.add_variable("e_GridPositionY", dummy);
    symbolTable.add_variable("e_GridPositionZ", dummy);
    symbolTable.add_variable("e_WorldX", dummy);
    symbolTable.add_variable("e_WorldY", dummy);
    symbolTable.add_variable("e_WorldZ", dummy);
    symbolTable.add_variable("e_CurrentHP", dummy);
    symbolTable.add_variable("e_MaxHP", dummy);
    symbolTable.add_variable("e_CurrentES", dummy);
    symbolTable.add_variable("e_MaxES", dummy);
    symbolTable.add_variable("e_IsSleeping", dummy);
    symbolTable.add_variable("e_CursorDistPx", dummy);
    symbolTable.add_variable("e_Reaction", dummy);
    symbolTable.add_variable("e_IsTargetable", dummy);
    symbolTable.add_variable("curX", dummy);
    symbolTable.add_variable("curY", dummy);
    symbolTable.add_constants();

    expression.register_symbol_table(symbolTable);

    // Preprocess so string-arg helpers validate too.
    std::string compiled;
    std::vector<std::string> bn, pn;
    if (!PreprocessExpression(expr, compiled, bn, pn, errorMsg)) return false;

    // Register dummy 1-arg functions to satisfy the parser.
    symbolTable.add_function("hasBuffIdx", &Dummy1Thunk);
    symbolTable.add_function("hasBuffValueIdx", &Dummy1Thunk);
    symbolTable.add_function("pathContainsIdx", &Dummy1Thunk);

    bool success = parser.compile(compiled, expression);
    if (!success) {
        errorMsg = "Expression syntax error. Translated: " + compiled;
    }
    return success;
}

// ============================================================================
// Global singleton
// ============================================================================

ScriptEngine& GetScriptEngine() {
    static ScriptEngine instance;
    return instance;
}