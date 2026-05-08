// AutoReflex - ScriptEngineDslPreprocessor
// DSL preprocessing helpers used by the scripting engine compilation/validation path.

#pragma once

#include <string>
#include <vector>

namespace AutoReflex::Scripting::Internal {

/**
 * Preprocesses a raw user expression string into an EXPRTK-ready expression string.
 *
 * @param rawExpressionString User-authored rule string (may include DSL helpers like `monsterCount.*`).
 * @param outPreprocessedExpressionString Output preprocessed expression string to compile with EXPRTK.
 * @param outBuffNeedles Output set of unique buff needles referenced by the expression.
 * @param outPathNeedles Output set of unique path needles referenced by the expression.
 * @param outErrorMessage On failure, receives a user-facing error message.
 * @returns True on success; otherwise false.
 */
bool PreprocessUserExpressionStringToExprtkExpressionString(
    const std::string& rawExpressionString,
    std::string& outPreprocessedExpressionString,
    std::vector<std::string>& outBuffNeedles,
    std::vector<std::string>& outPathNeedles,
    std::string& outErrorMessage);

} // namespace AutoReflex::Scripting::Internal

