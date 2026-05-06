// AutoReflex - KeySender
// Synthetic key presses via SendInput (global; the host has no role here).

#pragma once

#include <windows.h>

namespace AutoReflex {
namespace Game {

// Press and release a single virtual key (e.g. 'Q', VK_SPACE, VK_LBUTTON).
inline void PressKey(WORD vk)
{
    INPUT input = {0};
    input.type     = INPUT_KEYBOARD;
    input.ki.wVk   = vk;

    SendInput(1, &input, sizeof(INPUT));

    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

} // namespace Game
} // namespace AutoReflex
