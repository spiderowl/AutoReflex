// AutoReflex - MonsterHelpers
// Small utility helpers for game code. Kept lean: only what is currently
// used on the live code path.

#pragma once

#include <Windows.h>
#include <string>

namespace AutoReflex { namespace Game {

/**
 * Converts a UTF-16 wide string to a UTF-8 narrow string.
 *
 * @param wideStringUtf16 Input UTF-16 string.
 * @returns UTF-8 encoded string, with a lossy '?' fallback for unmappable characters.
 */
inline std::string ConvertWideStringUtf16ToUtf8String(const std::wstring& wideStringUtf16)
{
    if (wideStringUtf16.empty()) return {};
    const int utf8ByteCount = WideCharToMultiByte(
        CP_UTF8,
        0,
        wideStringUtf16.data(),
        static_cast<int>(wideStringUtf16.size()),
                                  nullptr, 0, nullptr, nullptr);
    if (utf8ByteCount <= 0) {
        std::string lossyAsciiString(wideStringUtf16.size(), ' ');
        for (size_t wideStringIndex = 0; wideStringIndex < wideStringUtf16.size(); ++wideStringIndex) {
            lossyAsciiString[wideStringIndex] =
                (wideStringUtf16[wideStringIndex] < 128)
                    ? static_cast<char>(wideStringUtf16[wideStringIndex])
                    : '?';
        }
        return lossyAsciiString;
    }
    std::string utf8String(static_cast<size_t>(utf8ByteCount), ' ');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wideStringUtf16.data(),
        static_cast<int>(wideStringUtf16.size()),
        utf8String.data(),
        utf8ByteCount,
        nullptr,
        nullptr);
    return utf8String;
}

}} // namespace AutoReflex::Game
