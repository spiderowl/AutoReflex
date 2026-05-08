// AutoReflex - ScriptEngineDslPreprocessor.cpp

#include "ScriptEngineDslPreprocessor.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace AutoReflex::Scripting::Internal {

namespace {

bool ExtractQuotedStringArgumentFromPosition(
    const std::string& inputString,
    size_t startQuoteIndex,
    std::string& outExtractedString,
    size_t& outNextIndexAfterClosingQuote)
{
    if (startQuoteIndex >= inputString.size() || inputString[startQuoteIndex] != '"') return false;
    size_t inputIndex = startQuoteIndex + 1;
    std::string buffer;
    buffer.reserve(32);
    while (inputIndex < inputString.size()) {
        const char currentCharacter = inputString[inputIndex];
        if (currentCharacter == '\\' && inputIndex + 1 < inputString.size()) {
            const char nextCharacter = inputString[inputIndex + 1];
            if (nextCharacter == '"' || nextCharacter == '\\') {
                buffer.push_back(nextCharacter);
                inputIndex += 2;
                continue;
            }
        }
        if (currentCharacter == '"') {
            outExtractedString = buffer;
            outNextIndexAfterClosingQuote = inputIndex + 1;
            return true;
        }
        buffer.push_back(currentCharacter);
        ++inputIndex;
    }
    return false;
}

int InternUniqueNeedleAndReturnIndex(std::vector<std::string>& inOutNeedlePool, const std::string& needle)
{
    for (size_t needleIndex = 0; needleIndex < inOutNeedlePool.size(); ++needleIndex) {
        if (inOutNeedlePool[needleIndex] == needle) return static_cast<int>(needleIndex);
    }
    inOutNeedlePool.push_back(needle);
    return static_cast<int>(inOutNeedlePool.size() - 1);
}

void SkipAsciiWhitespace(const std::string& inputString, size_t& inOutIndex)
{
    while (inOutIndex < inputString.size()) {
        const char currentCharacter = inputString[inOutIndex];
        if (currentCharacter == ' ' || currentCharacter == '\t' || currentCharacter == '\r' || currentCharacter == '\n') {
            ++inOutIndex;
            continue;
        }
        break;
    }
}

std::string TrimAsciiWhitespace(const std::string& inputString)
{
    size_t leftIndex = 0;
    while (leftIndex < inputString.size()) {
        const char currentCharacter = inputString[leftIndex];
        if (currentCharacter == ' ' || currentCharacter == '\t' || currentCharacter == '\r' || currentCharacter == '\n') {
            ++leftIndex;
            continue;
        }
        break;
    }
    size_t rightIndex = inputString.size();
    while (rightIndex > leftIndex) {
        const char currentCharacter = inputString[rightIndex - 1];
        if (currentCharacter == ' ' || currentCharacter == '\t' || currentCharacter == '\r' || currentCharacter == '\n') {
            --rightIndex;
            continue;
        }
        break;
    }
    return inputString.substr(leftIndex, rightIndex - leftIndex);
}

std::vector<std::string> SplitAndTrimAsciiByPipe(const std::string& inputString)
{
    std::vector<std::string> outputParts;
    size_t partStartIndex = 0;
    while (partStartIndex < inputString.size()) {
        size_t partEndIndex = partStartIndex;
        while (partEndIndex < inputString.size() && inputString[partEndIndex] != '|') ++partEndIndex;
        outputParts.push_back(TrimAsciiWhitespace(inputString.substr(partStartIndex, partEndIndex - partStartIndex)));
        partStartIndex = (partEndIndex < inputString.size()) ? (partEndIndex + 1) : partEndIndex;
    }
    outputParts.erase(
        std::remove_if(outputParts.begin(), outputParts.end(), [](const std::string& part) { return part.empty(); }),
        outputParts.end());
    return outputParts;
}

bool ConvertRarityTokenToValueAndAtLeastFlag(
    const std::string& token,
    int& outRarityValue,
    bool& outIsAtLeastToken)
{
    outIsAtLeastToken = false;
    if (token == "any")          { outRarityValue = 0; return true; }
    if (token == "normal")       { outRarityValue = 0; return true; }
    if (token == "magic")        { outRarityValue = 1; return true; }
    if (token == "rare")         { outRarityValue = 2; return true; }
    if (token == "unique")       { outRarityValue = 3; return true; }
    if (token == "atleastmagic") { outRarityValue = 1; outIsAtLeastToken = true; return true; }
    if (token == "atleastrare")  { outRarityValue = 2; outIsAtLeastToken = true; return true; }
    if (token == "atleastunique"){ outRarityValue = 3; outIsAtLeastToken = true; return true; }
    return false;
}

bool ParseAsciiIdentifierToken(const std::string& inputString, size_t& inOutIndex, std::string& outIdentifier)
{
    const size_t startIndex = inOutIndex;
    while (inOutIndex < inputString.size()) {
        const char currentCharacter = inputString[inOutIndex];
        const bool isIdentifierCharacter =
            (currentCharacter >= 'a' && currentCharacter <= 'z') ||
            (currentCharacter >= 'A' && currentCharacter <= 'Z') ||
            (currentCharacter >= '0' && currentCharacter <= '9') ||
            (currentCharacter == '_');
        if (!isIdentifierCharacter) break;
        ++inOutIndex;
    }
    if (inOutIndex == startIndex) return false;
    outIdentifier.assign(inputString.begin() + startIndex, inputString.begin() + inOutIndex);
    return true;
}

bool ParseSingleParenthesizedArgument(
    const std::string& inputString,
    size_t& inOutIndex,
    std::string& outArgumentString,
    std::string& outErrorMessage)
{
    SkipAsciiWhitespace(inputString, inOutIndex);
    if (inOutIndex >= inputString.size() || inputString[inOutIndex] != '(') return false;
    ++inOutIndex;
    SkipAsciiWhitespace(inputString, inOutIndex);
    if (inOutIndex >= inputString.size()) { outErrorMessage = "Missing ')'"; return false; }
    if (inputString[inOutIndex] == ')') { outArgumentString.clear(); ++inOutIndex; return true; }

    if (inputString[inOutIndex] == '"') {
        size_t nextIndex = inOutIndex;
        std::string extractedNeedle;
        if (!ExtractQuotedStringArgumentFromPosition(inputString, inOutIndex, extractedNeedle, nextIndex)) {
            outErrorMessage = "String is not closed";
            return false;
        }
        outArgumentString = extractedNeedle;
        inOutIndex = nextIndex;
    } else {
        const size_t argumentStartIndex = inOutIndex;
        while (inOutIndex < inputString.size() && inputString[inOutIndex] != ')') ++inOutIndex;
        if (inOutIndex >= inputString.size()) { outErrorMessage = "Missing ')'"; return false; }
        outArgumentString = TrimAsciiWhitespace(inputString.substr(argumentStartIndex, inOutIndex - argumentStartIndex));
    }

    SkipAsciiWhitespace(inputString, inOutIndex);
    if (inOutIndex >= inputString.size() || inputString[inOutIndex] != ')') { outErrorMessage = "Missing ')'"; return false; }
    ++inOutIndex;
    return true;
}

bool ParseCommaSeparatedParenthesizedArguments(
    const std::string& inputString,
    size_t& inOutIndex,
    std::vector<std::string>& outArgumentStrings,
    std::string& outErrorMessage)
{
    outArgumentStrings.clear();
    SkipAsciiWhitespace(inputString, inOutIndex);
    if (inOutIndex >= inputString.size() || inputString[inOutIndex] != '(') return false;
    ++inOutIndex;

    while (inOutIndex < inputString.size()) {
        SkipAsciiWhitespace(inputString, inOutIndex);
        if (inOutIndex >= inputString.size()) { outErrorMessage = "Missing ')'"; return false; }
        if (inputString[inOutIndex] == ')') { ++inOutIndex; break; }

        std::string argument;
        if (inputString[inOutIndex] == '"') {
            size_t nextIndex = inOutIndex;
            std::string extractedNeedle;
            if (!ExtractQuotedStringArgumentFromPosition(inputString, inOutIndex, extractedNeedle, nextIndex)) {
                outErrorMessage = "String is not closed";
                return false;
            }
            argument = extractedNeedle;
            inOutIndex = nextIndex;
        } else {
            const size_t argumentStartIndex = inOutIndex;
            while (inOutIndex < inputString.size() && inputString[inOutIndex] != ',' && inputString[inOutIndex] != ')') ++inOutIndex;
            if (inOutIndex > argumentStartIndex) {
                argument = TrimAsciiWhitespace(inputString.substr(argumentStartIndex, inOutIndex - argumentStartIndex));
            } else {
                outErrorMessage = "Missing argument";
                return false;
            }
        }

        outArgumentStrings.push_back(argument);
        SkipAsciiWhitespace(inputString, inOutIndex);
        if (inOutIndex >= inputString.size()) { outErrorMessage = "Missing ')'"; return false; }
        if (inputString[inOutIndex] == ',') { ++inOutIndex; continue; }
        if (inputString[inOutIndex] == ')') { ++inOutIndex; break; }
        outErrorMessage = "Expected ',' or ')'";
        return false;
    }
    return true;
}

bool TranslateMonsterCountChainWithRootAndDefaultReaction(
    const char* rootToken,
    int defaultReactionValue,
    const std::string& inputString,
    size_t& inOutIndex,
    std::string& outTranslatedExpressionString,
    std::vector<std::string>& inOutBuffNeedles,
    std::vector<std::string>& inOutPathNeedles,
    std::string& outErrorMessage)
{
    const size_t rootTokenLength = std::strlen(rootToken);
    static constexpr int kDefaultNearCursorPixels = 200;

    if (inputString.compare(inOutIndex, rootTokenLength, rootToken) != 0) return false;
    size_t parseIndex = inOutIndex + rootTokenLength;

    std::vector<std::string> coreConditions;
    std::vector<std::string> aimConditions;
    std::vector<std::string> buffConditions;
    coreConditions.reserve(10);
    aimConditions.reserve(2);
    buffConditions.reserve(6);

    coreConditions.push_back("(e_Reaction==" + std::to_string(defaultReactionValue) + ")");
    coreConditions.push_back("(e_CurrentHP>0)");
    coreConditions.push_back("(e_IsSleeping==0)");

    bool hasExplicitNearCursorMethod = false;
    std::string aimLimitSquaredExpressionString = "((200)*(200))";

    while (parseIndex < inputString.size()) {
        SkipAsciiWhitespace(inputString, parseIndex);
        if (parseIndex >= inputString.size() || inputString[parseIndex] != '.') break;
        ++parseIndex;
        SkipAsciiWhitespace(inputString, parseIndex);

        std::string methodName;
        if (!ParseAsciiIdentifierToken(inputString, parseIndex, methodName)) {
            outErrorMessage = "Expected method after '.'";
            return false;
        }

        std::string singleArgument;
        std::vector<std::string> argumentList;
        if (methodName == "hasBuffValue") {
            if (!ParseCommaSeparatedParenthesizedArguments(inputString, parseIndex, argumentList, outErrorMessage)) {
                outErrorMessage = "Expected '(...)' after method";
                return false;
            }
        } else {
            if (!ParseSingleParenthesizedArgument(inputString, parseIndex, singleArgument, outErrorMessage)) {
                outErrorMessage = "Expected '(...)' after method";
                return false;
            }
        }

        if (methodName == "nearCursor") {
            hasExplicitNearCursorMethod = true;
            aimLimitSquaredExpressionString = "((" + singleArgument + ")*(" + singleArgument + "))";
            aimConditions.push_back("(e_CursorDistSq<=(" + aimLimitSquaredExpressionString + "))");
        } else if (methodName == "hasBuff") {
            const int buffNeedleIndex = InternUniqueNeedleAndReturnIndex(inOutBuffNeedles, singleArgument);
            buffConditions.push_back(
                "hasBuffIdxGate(" + std::to_string(buffNeedleIndex) + "," + aimLimitSquaredExpressionString + ")");
        } else if (methodName == "notHasBuff") {
            const int buffNeedleIndex = InternUniqueNeedleAndReturnIndex(inOutBuffNeedles, singleArgument);
            buffConditions.push_back(
                "(hasBuffIdxGate(" + std::to_string(buffNeedleIndex) + "," + aimLimitSquaredExpressionString + ")==0)");
        } else if (methodName == "hasName") {
            const int pathNeedleIndex = InternUniqueNeedleAndReturnIndex(inOutPathNeedles, singleArgument);
            coreConditions.push_back("pathContainsIdx(" + std::to_string(pathNeedleIndex) + ")");
        } else if (methodName == "hasBuffValue") {
            if (argumentList.size() != 2) {
                outErrorMessage = "hasBuffValue() expects 2 args: \"name\",number";
                return false;
            }
            const int buffNeedleIndex = InternUniqueNeedleAndReturnIndex(inOutBuffNeedles, argumentList[0]);
            buffConditions.push_back(
                "(hasBuffValueIdxGate(" + std::to_string(buffNeedleIndex) + "," + aimLimitSquaredExpressionString + ")==" +
                argumentList[1] + ")");
        } else if (methodName == "type") {
            auto tokens = SplitAndTrimAsciiByPipe(singleArgument);
            if (tokens.empty()) { outErrorMessage = "type() expects a rarity token"; return false; }

            int minimumRarityValue = -1;
            bool hasAtLeastToken = false;
            std::vector<int> exactMatchRarityValues;

            for (auto& rarityToken : tokens) {
                int rarityValue = 0;
                bool isAtLeastToken = false;
                if (!ConvertRarityTokenToValueAndAtLeastFlag(rarityToken, rarityValue, isAtLeastToken)) {
                    outErrorMessage = "Unknown type() value: " + rarityToken;
                    return false;
                }
                if (rarityToken == "any") continue;
                if (isAtLeastToken) {
                    hasAtLeastToken = true;
                    if (rarityValue > minimumRarityValue) minimumRarityValue = rarityValue;
                } else {
                    exactMatchRarityValues.push_back(rarityValue);
                }
            }

            if (hasAtLeastToken) {
                coreConditions.push_back("(e_Rarity>=" + std::to_string(minimumRarityValue) + ")");
            }
            if (!exactMatchRarityValues.empty()) {
                std::string rarityExpressionString = "(";
                for (size_t valueIndex = 0; valueIndex < exactMatchRarityValues.size(); ++valueIndex) {
                    if (valueIndex) rarityExpressionString += " or ";
                    rarityExpressionString += "(e_Rarity==" + std::to_string(exactMatchRarityValues[valueIndex]) + ")";
                }
                rarityExpressionString += ")";
                coreConditions.push_back(rarityExpressionString);
            }
        } else {
            outErrorMessage = "Unknown monsterCount method: " + methodName;
            return false;
        }
    }

    if (!hasExplicitNearCursorMethod) {
        const std::string defaultNearCursorString = std::to_string(kDefaultNearCursorPixels);
        aimLimitSquaredExpressionString =
            "((" + defaultNearCursorString + ")*(" + defaultNearCursorString + "))";
        aimConditions.push_back("(e_CursorDistSq<=(" + aimLimitSquaredExpressionString + "))");
    }

    const int hiddenMonsterBuffNeedleIndex =
        InternUniqueNeedleAndReturnIndex(inOutBuffNeedles, "hidden_monster");
    buffConditions.push_back(
        "(hasBuffIdxGate(" + std::to_string(hiddenMonsterBuffNeedleIndex) + "," + aimLimitSquaredExpressionString + ")==0)");

    std::vector<std::string> allConditions;
    allConditions.reserve(coreConditions.size() + aimConditions.size() + buffConditions.size());
    allConditions.insert(allConditions.end(), coreConditions.begin(), coreConditions.end());
    allConditions.insert(allConditions.end(), aimConditions.begin(), aimConditions.end());
    allConditions.insert(allConditions.end(), buffConditions.begin(), buffConditions.end());

    outTranslatedExpressionString = "(";
    for (size_t conditionIndex = 0; conditionIndex < allConditions.size(); ++conditionIndex) {
        if (conditionIndex) outTranslatedExpressionString += " and ";
        outTranslatedExpressionString += allConditions[conditionIndex];
    }
    outTranslatedExpressionString += ")";

    inOutIndex = parseIndex;
    return true;
}

bool TranslateHostileMonsterCountChain(
    const std::string& inputString,
    size_t& inOutIndex,
    std::string& outTranslatedExpressionString,
    std::vector<std::string>& inOutBuffNeedles,
    std::vector<std::string>& inOutPathNeedles,
    std::string& outErrorMessage)
{
    return TranslateMonsterCountChainWithRootAndDefaultReaction(
        "monsterCount",
        0,
        inputString,
        inOutIndex,
        outTranslatedExpressionString,
        inOutBuffNeedles,
        inOutPathNeedles,
        outErrorMessage);
}

bool TranslateFriendlyMonsterCountChain(
    const std::string& inputString,
    size_t& inOutIndex,
    std::string& outTranslatedExpressionString,
    std::vector<std::string>& inOutBuffNeedles,
    std::vector<std::string>& inOutPathNeedles,
    std::string& outErrorMessage)
{
    return TranslateMonsterCountChainWithRootAndDefaultReaction(
        "friendlyMonsterCount",
        2,
        inputString,
        inOutIndex,
        outTranslatedExpressionString,
        inOutBuffNeedles,
        inOutPathNeedles,
        outErrorMessage);
}

} // namespace

bool PreprocessUserExpressionStringToExprtkExpressionString(
    const std::string& rawExpressionString,
    std::string& outPreprocessedExpressionString,
    std::vector<std::string>& outBuffNeedles,
    std::vector<std::string>& outPathNeedles,
    std::string& outErrorMessage)
{
    outPreprocessedExpressionString.clear();
    outPreprocessedExpressionString.reserve(rawExpressionString.size());

    size_t inputIndex = 0;
    while (inputIndex < rawExpressionString.size()) {
        std::string translatedExpression;
        size_t savedIndex = inputIndex;
        if (TranslateHostileMonsterCountChain(
                rawExpressionString,
                savedIndex,
                translatedExpression,
                outBuffNeedles,
                outPathNeedles,
                outErrorMessage)) {
            outPreprocessedExpressionString += translatedExpression;
            inputIndex = savedIndex;
            continue;
        }
        if (TranslateFriendlyMonsterCountChain(
                rawExpressionString,
                savedIndex,
                translatedExpression,
                outBuffNeedles,
                outPathNeedles,
                outErrorMessage)) {
            outPreprocessedExpressionString += translatedExpression;
            inputIndex = savedIndex;
            continue;
        }

        if (rawExpressionString.compare(inputIndex, 8, "hasBuff(") == 0) {
            size_t parseIndex = inputIndex + 8;
            while (parseIndex < rawExpressionString.size() && (rawExpressionString[parseIndex] == ' ' || rawExpressionString[parseIndex] == '\t')) ++parseIndex;
            if (parseIndex >= rawExpressionString.size() || rawExpressionString[parseIndex] != '"') { outErrorMessage = "hasBuff() expects a quoted string"; return false; }
            std::string needle;
            size_t nextIndex = parseIndex;
            if (!ExtractQuotedStringArgumentFromPosition(rawExpressionString, parseIndex, needle, nextIndex)) { outErrorMessage = "hasBuff() string is not closed"; return false; }
            while (nextIndex < rawExpressionString.size() && (rawExpressionString[nextIndex] == ' ' || rawExpressionString[nextIndex] == '\t')) ++nextIndex;
            if (nextIndex >= rawExpressionString.size() || rawExpressionString[nextIndex] != ')') { outErrorMessage = "hasBuff() missing ')'"; return false; }
            const int buffNeedleIndex = InternUniqueNeedleAndReturnIndex(outBuffNeedles, needle);
            outPreprocessedExpressionString += "hasBuffIdx(" + std::to_string(buffNeedleIndex) + ")";
            inputIndex = nextIndex + 1;
            continue;
        }

        if (rawExpressionString.compare(inputIndex, 12, "hasBuffValue(") == 0) {
            size_t parseIndex = inputIndex + 12;
            while (parseIndex < rawExpressionString.size() && (rawExpressionString[parseIndex] == ' ' || rawExpressionString[parseIndex] == '\t')) ++parseIndex;
            if (parseIndex >= rawExpressionString.size() || rawExpressionString[parseIndex] != '"') { outErrorMessage = "hasBuffValue() expects a quoted string"; return false; }
            std::string needle;
            size_t nextIndex = parseIndex;
            if (!ExtractQuotedStringArgumentFromPosition(rawExpressionString, parseIndex, needle, nextIndex)) { outErrorMessage = "hasBuffValue() string is not closed"; return false; }
            while (nextIndex < rawExpressionString.size() && (rawExpressionString[nextIndex] == ' ' || rawExpressionString[nextIndex] == '\t')) ++nextIndex;
            if (nextIndex >= rawExpressionString.size() || rawExpressionString[nextIndex] != ')') { outErrorMessage = "hasBuffValue() missing ')'"; return false; }
            const int buffNeedleIndex = InternUniqueNeedleAndReturnIndex(outBuffNeedles, needle);
            outPreprocessedExpressionString += "hasBuffValueIdx(" + std::to_string(buffNeedleIndex) + ")";
            inputIndex = nextIndex + 1;
            continue;
        }

        if (rawExpressionString.compare(inputIndex, 13, "pathContains(") == 0) {
            size_t parseIndex = inputIndex + 13;
            while (parseIndex < rawExpressionString.size() && (rawExpressionString[parseIndex] == ' ' || rawExpressionString[parseIndex] == '\t')) ++parseIndex;
            if (parseIndex >= rawExpressionString.size() || rawExpressionString[parseIndex] != '"') { outErrorMessage = "pathContains() expects a quoted string"; return false; }
            std::string needle;
            size_t nextIndex = parseIndex;
            if (!ExtractQuotedStringArgumentFromPosition(rawExpressionString, parseIndex, needle, nextIndex)) { outErrorMessage = "pathContains() string is not closed"; return false; }
            while (nextIndex < rawExpressionString.size() && (rawExpressionString[nextIndex] == ' ' || rawExpressionString[nextIndex] == '\t')) ++nextIndex;
            if (nextIndex >= rawExpressionString.size() || rawExpressionString[nextIndex] != ')') { outErrorMessage = "pathContains() missing ')'"; return false; }
            const int pathNeedleIndex = InternUniqueNeedleAndReturnIndex(outPathNeedles, needle);
            outPreprocessedExpressionString += "pathContainsIdx(" + std::to_string(pathNeedleIndex) + ")";
            inputIndex = nextIndex + 1;
            continue;
        }

        outPreprocessedExpressionString.push_back(rawExpressionString[inputIndex]);
        ++inputIndex;
    }

    outErrorMessage.clear();
    return true;
}

} // namespace AutoReflex::Scripting::Internal

