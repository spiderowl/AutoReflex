// AutoReflex - MonsterHelpers
// Small utility helpers for game code. Kept lean: only what is currently
// used on the live code path.

#pragma once

#include <Windows.h>
#include <string>

namespace AutoReflex { namespace Game {

// Narrow a std::wstring to UTF-8 (lossy fallback to '?' for unmappable chars).
// Used by the script engine to compare entity Path strings against case-
// insensitive needles supplied by the rule DSL (`hasName(...)`).
inline std::string WStringToString(const std::wstring& ws)
{
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
                                  nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        std::string out(ws.size(), ' ');
        for (size_t i = 0; i < ws.size(); ++i)
            out[i] = (ws[i] < 128) ? static_cast<char>(ws[i]) : '?';
        return out;
    }
    std::string out(static_cast<size_t>(len), ' ');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
                        out.data(), len, nullptr, nullptr);
    return out;
}

}} // namespace AutoReflex::Game
