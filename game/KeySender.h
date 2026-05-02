// AutoReflex - KeySender (Phase 3)
// Simulates key presses using SendInput (Win32 API)
// T21: PressKey(WORD vk) using SendInput keydown + keyup pair

#pragma once

#include <windows.h>

namespace AutoReflex {
namespace Game {

// T21: Simulate a single key press (keydown + keyup) using SendInput.
// vk is the virtual-key code (e.g. 'Q', VK_SPACE, 0x01 for LButton).
inline void PressKey(WORD vk)
{
    INPUT zeroInitializedInput = {0};
    zeroInitializedInput.type = INPUT_KEYBOARD;
    zeroInitializedInput.ki.wVk = vk;

    // Key down
    SendInput(1, &zeroInitializedInput, sizeof(INPUT));

    // Key up
    zeroInitializedInput.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &zeroInitializedInput, sizeof(INPUT));
}

// Legacy signature kept for compatibility with existing callers.
// Uses SendInput internally (ignores ctx since SendInput is global).
void ExecuteKeyPress(int virtualKeyCode, float holdDuration, void* ctx);

} // namespace Game
} // namespace AutoReflex
