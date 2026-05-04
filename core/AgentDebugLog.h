#pragma once
// #region agent log
#include <chrono>
#include <fstream>
#include <string>

inline std::string ArJsonEsc(std::string s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (c == '\\' || c == '"') {
            o += '\\';
            o += static_cast<char>(c);
        } else if (c < 32)
            o += ' ';
        else
            o += static_cast<char>(c);
    }
    return o;
}

inline void ArAgentNdjsonLog(const char* hypothesisId, const char* location, const std::string& message,
    const std::string& dataJson = "{}") {
    std::ofstream out(R"(c:\auto_help\Workspace\debug-e26960.log)", std::ios::app | std::ios::binary);
    if (!out)
        return;
    const auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
    out << "{\"sessionId\":\"e26960\",\"hypothesisId\":\"" << ArJsonEsc(hypothesisId) << "\",\"location\":\""
        << ArJsonEsc(location) << "\",\"message\":\"" << ArJsonEsc(message) << "\",\"data\":" << dataJson
        << ",\"timestamp\":" << ts << "}\n";
}
// #endregion
