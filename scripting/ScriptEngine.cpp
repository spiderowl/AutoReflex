// AutoReflex - ScriptEngine.cpp
// EXPRTK-based expression evaluation engine. Header-only library — no
// external linking required. Hot path is CompiledExpression::Evaluate().

#include "ScriptEngine.h"
#include "../sdk/PluginContext.h"
#include "../game/MonsterHelpers.h"

#include "exprtk.hpp"

#include <imgui.h>

#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#if defined(_DEBUG) || defined(AUTOREFLEX_ENABLE_BUFFS_DUMP)
#define AUTOREFLEX_BUFFS_DUMP 1
#include <fstream>
#else
#define AUTOREFLEX_BUFFS_DUMP 0
#endif
#include <mutex>
#include <unordered_set>
#include <chrono>
#include <filesystem>
#include <cstring>

using symbol_table_t = exprtk::symbol_table<double>;
using expression_t   = exprtk::expression<double>;
using parser_t       = exprtk::parser<double>;

CompiledExpression::CompiledExpression()
    : expression_(std::make_unique<expression_t>())
    , symbolTable_(std::make_unique<symbol_table_t>())
    , parser_(std::make_unique<parser_t>())
{
    symbolTable_->add_variable("e_Id",            e_Id);
    symbolTable_->add_variable("e_IsValid",       e_IsValid);
    symbolTable_->add_variable("e_Rarity",        e_Rarity);
    symbolTable_->add_variable("e_EntityState",   e_EntityState);
    symbolTable_->add_variable("e_GridPositionX", e_GridPositionX);
    symbolTable_->add_variable("e_GridPositionY", e_GridPositionY);
    symbolTable_->add_variable("e_WorldX",        e_WorldX);
    symbolTable_->add_variable("e_WorldY",        e_WorldY);
    symbolTable_->add_variable("e_WorldZ",        e_WorldZ);
    symbolTable_->add_variable("e_CurrentHP",     e_CurrentHP);
    symbolTable_->add_variable("e_MaxHP",         e_MaxHP);
    symbolTable_->add_variable("e_CurrentES",     e_CurrentES);
    symbolTable_->add_variable("e_MaxES",         e_MaxES);
    symbolTable_->add_variable("e_IsSleeping",    e_IsSleeping);
    symbolTable_->add_variable("e_CursorDistPx",  e_CursorDistPx);
    symbolTable_->add_variable("e_CursorDistSq",  e_CursorDistSq);
    symbolTable_->add_variable("e_Reaction",      e_Reaction);

    symbolTable_->add_constants();
    expression_->register_symbol_table(*symbolTable_);
}

CompiledExpression::~CompiledExpression() = default;

namespace {

bool ExtractQuotedArg(const std::string& s, size_t startQuote, std::string& out, size_t& outNextPos) {
    if (startQuote >= s.size() || s[startQuote] != '"') return false;
    size_t i = startQuote + 1;
    std::string buf;
    buf.reserve(32);
    while (i < s.size()) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size()) {
            char n = s[i + 1];
            if (n == '"' || n == '\\') { buf.push_back(n); i += 2; continue; }
        }
        if (c == '"') { out = buf; outNextPos = i + 1; return true; }
        buf.push_back(c);
        ++i;
    }
    return false;
}

int InternNeedle(std::vector<std::string>& v, const std::string& needle) {
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == needle) return static_cast<int>(i);
    }
    v.push_back(needle);
    return static_cast<int>(v.size() - 1);
}

void SkipWs(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
}

std::string TrimAscii(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    size_t b = s.size();
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

std::vector<std::string> SplitByPipe(const std::string& s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = i;
        while (j < s.size() && s[j] != '|') ++j;
        out.push_back(TrimAscii(s.substr(i, j - i)));
        i = (j < s.size()) ? (j + 1) : j;
    }
    out.erase(std::remove_if(out.begin(), out.end(),
        [](const std::string& x) { return x.empty(); }), out.end());
    return out;
}

bool RarityTokenToValue(const std::string& token, int& outValue, bool& outAtLeast) {
    outAtLeast = false;
    if (token == "any")          { outValue = 0; return true; }
    if (token == "normal")       { outValue = 0; return true; }
    if (token == "magic")        { outValue = 1; return true; }
    if (token == "rare")         { outValue = 2; return true; }
    if (token == "unique")       { outValue = 3; return true; }
    if (token == "atleastmagic") { outValue = 1; outAtLeast = true; return true; }
    if (token == "atleastrare")  { outValue = 2; outAtLeast = true; return true; }
    if (token == "atleastunique"){ outValue = 3; outAtLeast = true; return true; }
    return false;
}

bool ParseIdentifier(const std::string& s, size_t& i, std::string& outIdent) {
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

bool ParseParenArg(const std::string& s, size_t& i, std::string& outArg, std::string& errorMsg) {
    SkipWs(s, i);
    if (i >= s.size() || s[i] != '(') return false;
    ++i;
    SkipWs(s, i);
    if (i >= s.size()) { errorMsg = "Missing ')'"; return false; }
    if (s[i] == ')') { outArg.clear(); ++i; return true; }

    if (s[i] == '"') {
        size_t nextPos = i;
        std::string needle;
        if (!ExtractQuotedArg(s, i, needle, nextPos)) { errorMsg = "String is not closed"; return false; }
        outArg = needle;
        i = nextPos;
    } else {
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

bool ParseParenArgs(const std::string& s, size_t& i, std::vector<std::string>& outArgs, std::string& errorMsg) {
    outArgs.clear();
    SkipWs(s, i);
    if (i >= s.size() || s[i] != '(') return false;
    ++i;

    while (i < s.size()) {
        SkipWs(s, i);
        if (i >= s.size()) { errorMsg = "Missing ')'"; return false; }
        if (s[i] == ')') { ++i; break; }

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

bool TranslateMonsterCountChainImpl(const char* root,
                                    int defaultReaction,
                                    const std::string& s,
                                    size_t& i,
                                    std::string& outExpr,
                                    std::vector<std::string>& buffNeedles,
                                    std::vector<std::string>& pathNeedles,
                                    std::string& errorMsg)
{
    const size_t kRootLen = std::strlen(root);
    // When `.nearCursor` is omitted, apply this default. Skills are cursor-aimed,
    // so the cursor-pixel radius is the only implicit spatial filter.
    static constexpr int kDefaultNearCursorPx = 200;

    if (s.compare(i, kRootLen, root) != 0) return false;
    size_t p = i + kRootLen;

    // Order matters for perf: core filters first, then aim (WorldToScreen),
    // then buff reads (ReadBuffsComponent) last.
    std::vector<std::string> coreConds;
    std::vector<std::string> aimConds;
    std::vector<std::string> buffConds;
    coreConds.reserve(10);
    aimConds.reserve(2);
    buffConds.reserve(6);

    coreConds.push_back("(e_Reaction==" + std::to_string(defaultReaction) + ")");
    coreConds.push_back("(e_CurrentHP>0)");
    coreConds.push_back("(e_IsSleeping==0)");
    int hiddenMonsterIdx = -1;

    bool sawNearCursor = false;
    // Expression string for N^2 used by buff-gated reads.
    // Default is the implicit nearCursor when the method is omitted.
    std::string aimLimitSqExpr = "((200)*(200))";

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

        if (method == "nearCursor") {
            sawNearCursor = true;
            aimLimitSqExpr = "((" + arg + ")*(" + arg + "))";
            aimConds.push_back("(e_CursorDistSq<=(" + aimLimitSqExpr + "))");
        } else if (method == "hasBuff") {
            int idx = InternNeedle(buffNeedles, arg);
            // Gate buff reads on aim radius even if the expression engine evaluates both sides.
            buffConds.push_back("hasBuffIdxGate(" + std::to_string(idx) + "," + aimLimitSqExpr + ")");
        } else if (method == "notHasBuff") {
            int idx = InternNeedle(buffNeedles, arg);
            buffConds.push_back("(hasBuffIdxGate(" + std::to_string(idx) + "," + aimLimitSqExpr + ")==0)");
        } else if (method == "hasName") {
            int idx = InternNeedle(pathNeedles, arg);
            coreConds.push_back("pathContainsIdx(" + std::to_string(idx) + ")");
        } else if (method == "hasBuffValue") {
            if (args.size() != 2) { errorMsg = "hasBuffValue() expects 2 args: \"name\",number"; return false; }
            int idx = InternNeedle(buffNeedles, args[0]);
            buffConds.push_back("(hasBuffValueIdxGate(" + std::to_string(idx) + "," + aimLimitSqExpr + ")==" + args[1] + ")");
        } else if (method == "type") {
            auto parts = SplitByPipe(arg);
            if (parts.empty()) { errorMsg = "type() expects a rarity token"; return false; }

            int atleastVal = -1;
            bool sawAtLeast = false;
            std::vector<int> equalsVals;

            for (auto& pTok : parts) {
                int v = 0; bool atLeast = false;
                if (!RarityTokenToValue(pTok, v, atLeast)) { errorMsg = "Unknown type() value: " + pTok; return false; }
                if (pTok == "any") continue;
                if (atLeast) {
                    sawAtLeast = true;
                    if (v > atleastVal) atleastVal = v;
                } else {
                    equalsVals.push_back(v);
                }
            }

            if (sawAtLeast) {
                coreConds.push_back("(e_Rarity>=" + std::to_string(atleastVal) + ")");
            }
            if (!equalsVals.empty()) {
                std::string expr = "(";
                for (size_t k = 0; k < equalsVals.size(); ++k) {
                    if (k) expr += " or ";
                    expr += "(e_Rarity==" + std::to_string(equalsVals[k]) + ")";
                }
                expr += ")";
                coreConds.push_back(expr);
            }
        } else {
            errorMsg = "Unknown monsterCount method: " + method;
            return false;
        }
    }

    if (!sawNearCursor) {
        const std::string n = std::to_string(kDefaultNearCursorPx);
        aimLimitSqExpr = "((" + n + ")*(" + n + "))";
        aimConds.push_back("(e_CursorDistSq<=(" + aimLimitSqExpr + "))");
    }

    // Dormant / unrevealed packs often carry hidden_monster on Buffs; exclude by default.
    hiddenMonsterIdx = InternNeedle(buffNeedles, "hidden_monster");
    buffConds.push_back("(hasBuffIdxGate(" + std::to_string(hiddenMonsterIdx) + "," + aimLimitSqExpr + ")==0)");

    std::vector<std::string> conds;
    conds.reserve(coreConds.size() + aimConds.size() + buffConds.size());
    conds.insert(conds.end(), coreConds.begin(), coreConds.end());
    conds.insert(conds.end(), aimConds.begin(), aimConds.end());
    conds.insert(conds.end(), buffConds.begin(), buffConds.end());

    outExpr = "(";
    for (size_t k = 0; k < conds.size(); ++k) {
        if (k) outExpr += " and ";
        outExpr += conds[k];
    }
    outExpr += ")";

    i = p;
    return true;
}

bool TranslateMonsterCountChain(const std::string& s,
                                size_t& i,
                                std::string& outExpr,
                                std::vector<std::string>& buffNeedles,
                                std::vector<std::string>& pathNeedles,
                                std::string& errorMsg)
{
    return TranslateMonsterCountChainImpl("monsterCount", 0, s, i, outExpr, buffNeedles, pathNeedles, errorMsg);
}

bool TranslateFriendlyMonsterCountChain(const std::string& s,
                                        size_t& i,
                                        std::string& outExpr,
                                        std::vector<std::string>& buffNeedles,
                                        std::vector<std::string>& pathNeedles,
                                        std::string& errorMsg)
{
    return TranslateMonsterCountChainImpl("friendlyMonsterCount", 2, s, i, outExpr, buffNeedles, pathNeedles, errorMsg);
}

bool PreprocessExpression(const std::string& in,
                          std::string& out,
                          std::vector<std::string>& buffNeedles,
                          std::vector<std::string>& pathNeedles,
                          std::string& errorMsg)
{
    out.clear();
    out.reserve(in.size());

    size_t i = 0;
    while (i < in.size()) {
        std::string translated;
        size_t save = i;
        if (TranslateMonsterCountChain(in, save, translated, buffNeedles, pathNeedles, errorMsg)) {
            out += translated;
            i = save;
            continue;
        }
        if (TranslateFriendlyMonsterCountChain(in, save, translated, buffNeedles, pathNeedles, errorMsg)) {
            out += translated;
            i = save;
            continue;
        }

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

// EXPRTK uses C function pointers for custom functions; thread_local pointer
// lets us route back to the right CompiledExpression instance.
thread_local const CompiledExpression* tl_expr = nullptr;

double HasBuffIdxThunk(double idx) {
    if (!tl_expr) return 0.0;
    return tl_expr->HasBuffIdx(static_cast<int>(idx)) ? 1.0 : 0.0;
}
double HasBuffValueIdxThunk(double idx) {
    if (!tl_expr) return 0.0;
    return tl_expr->HasBuffValueIdx(static_cast<int>(idx));
}
double HasBuffIdxGateThunk(double idx, double limitSq) {
    if (!tl_expr) return 0.0;
    return tl_expr->HasBuffIdxGate(static_cast<int>(idx), limitSq) ? 1.0 : 0.0;
}
double HasBuffValueIdxGateThunk(double idx, double limitSq) {
    if (!tl_expr) return 0.0;
    return tl_expr->HasBuffValueIdxGate(static_cast<int>(idx), limitSq);
}
double PathContainsIdxThunk(double idx) {
    if (!tl_expr) return 0.0;
    return tl_expr->PathContainsIdx(static_cast<int>(idx)) ? 1.0 : 0.0;
}
double Dummy1Thunk(double) { return 0.0; }
double Dummy2Thunk(double, double) { return 0.0; }

inline char AsciiToLower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
}

void LowerAsciiInPlace(std::string& s) {
    for (char& c : s) c = AsciiToLower(c);
}

bool Contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

#if AUTOREFLEX_BUFFS_DUMP
std::mutex g_buffsDumpMu;
std::string g_buffsDumpPath = "AutoReflex_BuffsDump.txt";
bool g_buffsDumpEnabled = false;
#endif

static bool IsTrueTag(const char* tag)
{
    if (!tag) return false;
    // Any line that proves the entity actually matched the expression.
    return std::string(tag).rfind("Evaluate_TRUE", 0) == 0;
}

uintptr_t TryGetBuffsAddrFromDebugList(PluginContext* ctx, uint32_t entityId)
{
    if (!ctx || !ctx->GetEntityDebugList) return 0;
    // Cache the debug list scan to avoid O(monsters * entities) work each tick.
    static std::mutex s_cacheMu;
    static std::unordered_map<uint32_t, uintptr_t> s_buffsAddrById;
    static uint64_t s_lastRefreshMs = 0;

    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const uint64_t nowMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());

    std::lock_guard<std::mutex> lock(s_cacheMu);
    // Refresh at most twice per second.
    if (nowMs - s_lastRefreshMs > 500) {
        s_buffsAddrById.clear();
        const auto list = ctx->GetEntityDebugList();
        s_buffsAddrById.reserve(list.size());
        for (const auto& e : list) {
            uintptr_t buffsAddr = 0;
            for (const auto& kv : e.ComponentAddresses) {
                std::string k = kv.first;
                LowerAsciiInPlace(k);
                if (k.find("buff") != std::string::npos) { buffsAddr = kv.second; break; }
            }
            if (buffsAddr) s_buffsAddrById.emplace(e.Id, buffsAddr);
        }
        s_lastRefreshMs = nowMs;
    }

    auto it = s_buffsAddrById.find(entityId);
    return (it == s_buffsAddrById.end()) ? 0 : it->second;
}

uintptr_t ResolveBuffsAddr(PluginContext* ctx, const PluginSDK::RadarEntity& ent, bool& outUsedFallback)
{
    outUsedFallback = false;
    uintptr_t addr = ent.ComponentCache.BuffsAddr;
    if (addr != 0) return addr;
    addr = TryGetBuffsAddrFromDebugList(ctx, static_cast<uint32_t>(ent.Id));
    if (addr != 0) outUsedFallback = true;
    return addr;
}

void AppendBuffsDebugLine(const PluginSDK::RadarEntity& ent,
                          const PluginSDK::PluginBuffsData* dataOrNull,
                          const char* tag)
{
#if !AUTOREFLEX_BUFFS_DUMP
    (void)ent; (void)dataOrNull; (void)tag;
    return;
#else
    // User-requested debugging: dump what ReadBuffsComponent returned.
    // Intentionally append-only and can be large.
    {
        std::lock_guard<std::mutex> lock(g_buffsDumpMu);
        if (!g_buffsDumpEnabled) return;
    }
    static std::mutex s_mu;
    static std::unordered_set<uint32_t> s_loggedOnce;

    const uint32_t id = static_cast<uint32_t>(ent.Id);
    {
        std::lock_guard<std::mutex> lock(s_mu);
        // Log each entity once per run, but ALWAYS log TRUE matches even if already seen.
        if (!IsTrueTag(tag) && !s_loggedOnce.insert(id).second) return;
    }

    std::string pathCopy;
    { std::lock_guard<std::mutex> lock(g_buffsDumpMu); pathCopy = g_buffsDumpPath; }

    // Ensure parent folder exists (we usually point at <pluginDir>/config/...).
    try {
        std::filesystem::path p(pathCopy);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    } catch (...) {
    }

    std::ofstream f(pathCopy.c_str(), std::ios::out | std::ios::app);
    if (!f.is_open()) return;

    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    f << "t_ms=" << ms
      << " tag=" << (tag ? tag : "")
      << " entId=" << ent.Id
      << " reaction=" << static_cast<int>(ent.Reaction)
      << " hp=" << ent.CurrentHP
      << " sleeping=" << (ent.IsSleeping ? 1 : 0)
      << " buffsAddr=0x" << std::hex << ent.ComponentCache.BuffsAddr << std::dec;

    if (!dataOrNull) {
        f << " valid=<null>\n";
        return;
    }

    f << " valid=" << (dataOrNull->Valid ? 1 : 0)
      << " buffCount=" << dataOrNull->Buffs.size()
      << "\n";

    for (size_t i = 0; i < dataOrNull->Buffs.size(); ++i) {
        f << "  [" << i << "] name=\"" << dataOrNull->Buffs[i].Name << "\"\n";
    }
#endif
}

} // anonymous namespace

void ScriptEngine::SetBuffsDumpPath(const std::string& path)
{
#if !AUTOREFLEX_BUFFS_DUMP
    (void)path;
    return;
#else
    if (path.empty()) return;
    std::lock_guard<std::mutex> lock(g_buffsDumpMu);
    g_buffsDumpPath = path;
#endif
}

void ScriptEngine::SetBuffsDumpEnabled(bool enabled)
{
#if !AUTOREFLEX_BUFFS_DUMP
    (void)enabled;
    return;
#else
    std::lock_guard<std::mutex> lock(g_buffsDumpMu);
    g_buffsDumpEnabled = enabled;
#endif
}


void CompiledExpression::ComputeNeedsFlags()
{
    needsCursorPx_   = compiledString_.find("e_CursorDistPx")  != std::string::npos;
    needsCursorSq_   = compiledString_.find("e_CursorDistSq")  != std::string::npos;
    needsBuffs_      = (compiledString_.find("hasBuffIdx(")          != std::string::npos)
                    || (compiledString_.find("hasBuffValueIdx(")     != std::string::npos)
                    || (compiledString_.find("hasBuffIdxGate(")      != std::string::npos)
                    || (compiledString_.find("hasBuffValueIdxGate(") != std::string::npos);
    needsPath_       = compiledString_.find("pathContainsIdx(") != std::string::npos;
    needsCursorForBuffGate_ = (compiledString_.find("hasBuffIdxGate(") != std::string::npos)
                           || (compiledString_.find("hasBuffValueIdxGate(") != std::string::npos);
}

bool CompiledExpression::Compile(const std::string& exprString, std::string& errorMsg)
{
    exprString_ = exprString;
    compiledString_.clear();
    buffNeedles_.clear();
    pathNeedles_.clear();
    pathNeedlesLower_.clear();

    if (!PreprocessExpression(exprString_, compiledString_, buffNeedles_, pathNeedles_, errorMsg)) {
        return false;
    }

    pathNeedlesLower_.reserve(pathNeedles_.size());
    for (const auto& n : pathNeedles_) {
        std::string lower = n;
        LowerAsciiInPlace(lower);
        pathNeedlesLower_.push_back(std::move(lower));
    }

    symbolTable_->add_function("hasBuffIdx",      &HasBuffIdxThunk);
    symbolTable_->add_function("hasBuffValueIdx", &HasBuffValueIdxThunk);
    symbolTable_->add_function("hasBuffIdxGate",      &HasBuffIdxGateThunk);
    symbolTable_->add_function("hasBuffValueIdxGate", &HasBuffValueIdxGateThunk);
    symbolTable_->add_function("pathContainsIdx", &PathContainsIdxThunk);

    if (!parser_->compile(compiledString_, *expression_)) {
        errorMsg = "Expression compilation failed. Translated: " + compiledString_;
        return false;
    }

    ComputeNeedsFlags();
    errorMsg.clear();
    return true;
}

bool CompiledExpression::Evaluate(PluginContext* ctx, const PluginSDK::RadarEntity& entity) const
{
    if (!expression_) return false;

    tl_expr = this;

    // Bind from RadarEntity directly — no intermediate copy.
    e_Id            = static_cast<double>(entity.Id);
    e_IsValid       = entity.IsValid ? 1.0 : 0.0;
    e_Rarity        = static_cast<double>(entity.Rarity);
    e_EntityState   = static_cast<double>(static_cast<int>(entity.entityState));
    e_GridPositionX = static_cast<double>(entity.GridPositionX);
    e_GridPositionY = static_cast<double>(entity.GridPositionY);
    e_WorldX        = static_cast<double>(entity.WorldX);
    e_WorldY        = static_cast<double>(entity.WorldY);
    e_WorldZ        = static_cast<double>(entity.WorldZ);
    e_CurrentHP     = static_cast<double>(entity.CurrentHP);
    e_MaxHP         = static_cast<double>(entity.MaxHP);
    e_CurrentES     = static_cast<double>(entity.CurrentES);
    e_MaxES         = static_cast<double>(entity.MaxES);
    e_IsSleeping    = entity.IsSleeping ? 1.0 : 0.0;
    e_Reaction      = static_cast<double>(entity.Reaction);

    curCtx_ = ctx;
    curEnt_ = &entity;
    buffsCacheReady_ = false;
    buffsCacheValid_ = false;
    buffsCacheUsedFallback_ = false;

    // Debug dump: only when enabled (otherwise it would dominate CPU).
    PluginSDK::PluginBuffsData dbgData{};
    PluginSDK::PluginBuffsData* dbgPtr = nullptr;
    bool usedFallback = false;
    uintptr_t resolvedBuffsAddr = 0;

    bool dumpEnabled = false;
#if AUTOREFLEX_BUFFS_DUMP
    { std::lock_guard<std::mutex> lock(g_buffsDumpMu); dumpEnabled = g_buffsDumpEnabled; }
#endif
    if (dumpEnabled) {
        if (ctx && ctx->ReadBuffsComponent) {
            resolvedBuffsAddr = ResolveBuffsAddr(ctx, entity, usedFallback);
            if (resolvedBuffsAddr != 0) {
                dbgData = ctx->ReadBuffsComponent(resolvedBuffsAddr);
                dbgPtr = &dbgData;
                AppendBuffsDebugLine(entity, dbgPtr, usedFallback ? "Evaluate_FallbackAddr" : "Evaluate");
            } else {
                AppendBuffsDebugLine(entity, nullptr, "Evaluate_NoBuffsAddr");
            }
        } else {
            AppendBuffsDebugLine(entity, nullptr, "Evaluate_NoBuffsAPI");
        }
    }

    // Reset per-evaluation needle caches only when actually used.
    if (needsBuffs_) {
        buffResultCache_.assign(buffNeedles_.size(), -1);
        buffValueCache_.assign(buffNeedles_.size(), static_cast<int16_t>(-32768));
    }
    if (needsPath_) {
        pathResultCache_.assign(pathNeedles_.size(), -1);
        pathLowerReady_ = false;
    }

    // Cursor distance: only compute if the expression needs it (directly or via buff gates).
    if ((needsCursorPx_ || needsCursorSq_ || needsCursorForBuffGate_) && ctx && ctx->WorldToScreen) {
        float sx = 0.f, sy = 0.f;
        if (ctx->WorldToScreen(entity.WorldX, entity.WorldY, entity.WorldZ, &sx, &sy)) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const float dx = sx - mouse.x;
            const float dy = sy - mouse.y;
            const double d2 = static_cast<double>(dx * dx + dy * dy);
            e_CursorDistSq = d2;
            if (needsCursorPx_) {
                e_CursorDistPx = std::sqrt(d2);
            }
        } else {
            e_CursorDistPx = 1.0e9;
            e_CursorDistSq = 1.0e18;
        }
    } else {
        e_CursorDistPx = 1.0e9;
        e_CursorDistSq = 1.0e18;
    }

    const double result = expression_->value();
    if (dumpEnabled && result != 0.0) {
        // When something *actually matches the rule*, ALWAYS log it (see IsTrueTag()).
        AppendBuffsDebugLine(entity, dbgPtr, usedFallback ? "Evaluate_TRUE_FallbackAddr" : "Evaluate_TRUE");
    }
    return result != 0.0;
}

const PluginSDK::PluginBuffsData* CompiledExpression::GetBuffsDataCached(bool& outUsedFallback) const
{
    outUsedFallback = false;
    if (!curCtx_ || !curEnt_) return nullptr;
    if (!curCtx_->ReadBuffsComponent) return nullptr;

    if (!buffsCacheReady_) {
        bool usedFallback = false;
        const uintptr_t buffsAddr = ResolveBuffsAddr(curCtx_, *curEnt_, usedFallback);
        buffsCacheUsedFallback_ = usedFallback;
        if (!buffsAddr) {
            buffsCacheValid_ = false;
            buffsCacheReady_ = true;
        } else {
            buffsCacheData_ = curCtx_->ReadBuffsComponent(buffsAddr);
            buffsCacheValid_ = buffsCacheData_.Valid;
            buffsCacheReady_ = true;
        }
    }

    outUsedFallback = buffsCacheUsedFallback_;
    return buffsCacheValid_ ? &buffsCacheData_ : nullptr;
}

bool CompiledExpression::HasBuffIdx(int idx) const
{
    if (!curCtx_ || !curEnt_) return false;
    if (idx < 0 || idx >= static_cast<int>(buffNeedles_.size())) return false;
    if (!curCtx_->ReadBuffsComponent) return false;

    if (idx < static_cast<int>(buffResultCache_.size()) && buffResultCache_[idx] != -1)
        return buffResultCache_[idx] == 1;

    bool usedFallback = false;
    const auto* data = GetBuffsDataCached(usedFallback);
    if (!data) {
        AppendBuffsDebugLine(*curEnt_, nullptr, "ReadBuffsComponent_NoAddr");
        if (idx < static_cast<int>(buffResultCache_.size())) buffResultCache_[idx] = 0;
        return false;
    }
    AppendBuffsDebugLine(*curEnt_, data, usedFallback ? "ReadBuffsComponent_FallbackAddr" : "ReadBuffsComponent");

    bool found = false;
    const auto& needle = buffNeedles_[idx];
    for (const auto& b : data->Buffs) {
        if (b.Name == needle) { found = true; break; }
    }
    if (idx < static_cast<int>(buffResultCache_.size())) buffResultCache_[idx] = found ? 1 : 0;
    return found;
}

bool CompiledExpression::HasBuffIdxGate(int idx, double limitSq) const
{
    // If the entity is outside the nearCursor radius, the overall monsterCount
    // expression should be false anyway; skip expensive buff reads.
    if (e_CursorDistSq > limitSq) return false;
    return HasBuffIdx(idx);
}

double CompiledExpression::HasBuffValueIdx(int idx) const
{
    if (!curCtx_ || !curEnt_) return 0.0;
    if (idx < 0 || idx >= static_cast<int>(buffNeedles_.size())) return 0.0;
    if (!curCtx_->ReadBuffsComponent) return 0.0;

    if (idx < static_cast<int>(buffValueCache_.size()) && buffValueCache_[idx] != static_cast<int16_t>(-32768))
        return static_cast<double>(buffValueCache_[idx]);

    bool usedFallback = false;
    const auto* data = GetBuffsDataCached(usedFallback);
    if (!data) {
        AppendBuffsDebugLine(*curEnt_, nullptr, "ReadBuffsComponent_NoAddr");
        if (idx < static_cast<int>(buffValueCache_.size())) buffValueCache_[idx] = 0;
        return 0.0;
    }
    AppendBuffsDebugLine(*curEnt_, data, usedFallback ? "ReadBuffsComponent_FallbackAddr" : "ReadBuffsComponent");

    int16_t value = 0;
    const auto& needle = buffNeedles_[idx];
    for (const auto& b : data->Buffs) {
        if (b.Name == needle) { value = static_cast<int16_t>(b.Charges); break; }
    }
    if (idx < static_cast<int>(buffValueCache_.size())) buffValueCache_[idx] = value;
    return static_cast<double>(value);
}

double CompiledExpression::HasBuffValueIdxGate(int idx, double limitSq) const
{
    if (e_CursorDistSq > limitSq) return 0.0;
    return HasBuffValueIdx(idx);
}

bool CompiledExpression::PathContainsIdx(int idx) const
{
    if (!curEnt_) return false;
    if (idx < 0 || idx >= static_cast<int>(pathNeedlesLower_.size())) return false;

    if (idx < static_cast<int>(pathResultCache_.size()) && pathResultCache_[idx] != -1)
        return pathResultCache_[idx] == 1;

    // Lazily build a lowercased copy of the entity path once per evaluation.
    if (!pathLowerReady_) {
        pathLowerScratch_ = AutoReflex::Game::WStringToString(curEnt_->Path);
        LowerAsciiInPlace(pathLowerScratch_);
        pathLowerReady_ = true;
    }

    const bool ok = Contains(pathLowerScratch_, pathNeedlesLower_[idx]);
    if (idx < static_cast<int>(pathResultCache_.size())) pathResultCache_[idx] = ok ? 1 : 0;
    return ok;
}

// ============================================================================
// ScriptEngine implementation
// ============================================================================

ScriptEngine::ScriptEngine()  = default;
ScriptEngine::~ScriptEngine() = default;

bool ScriptEngine::Initialize() {
    initialized_ = true;
    return true;
}

bool ScriptEngine::ValidateExpression(const std::string& expr, std::string& errorMsg) {
    symbol_table_t symbolTable;
    expression_t   expression;
    parser_t       parser;

    // ValidateExpression doesn't evaluate, so all variables share one dummy backing slot.
    double dummy = 0.0;
    static const char* const kVarNames[] = {
        "e_Id", "e_IsValid", "e_Rarity", "e_EntityState",
        "e_GridPositionX", "e_GridPositionY",
        "e_WorldX", "e_WorldY", "e_WorldZ",
        "e_CurrentHP", "e_MaxHP", "e_CurrentES", "e_MaxES",
        "e_IsSleeping", "e_CursorDistPx", "e_CursorDistSq", "e_Reaction",
    };
    for (const char* name : kVarNames) symbolTable.add_variable(name, dummy);
    symbolTable.add_constants();
    expression.register_symbol_table(symbolTable);

    std::string compiled;
    std::vector<std::string> bn, pn;
    if (!PreprocessExpression(expr, compiled, bn, pn, errorMsg)) return false;

    symbolTable.add_function("hasBuffIdx",      &Dummy1Thunk);
    symbolTable.add_function("hasBuffValueIdx", &Dummy1Thunk);
    symbolTable.add_function("hasBuffIdxGate",      &Dummy2Thunk);
    symbolTable.add_function("hasBuffValueIdxGate", &Dummy2Thunk);
    symbolTable.add_function("pathContainsIdx", &Dummy1Thunk);

    if (!parser.compile(compiled, expression)) {
        errorMsg = "Expression syntax error. Translated: " + compiled;
        return false;
    }
    return true;
}
