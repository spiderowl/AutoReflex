// AutoReflex - game/KeySender.cpp (Phase 3)
// T21: ExecuteKeyPress delegates to PressKey (SendInput)
#include "KeySender.h"
#include <thread>
#include <chrono>

namespace AutoReflex { namespace Game {

void ExecuteKeyPress(int virtualKeyCode, float holdDuration, void* /*ctx*/)
{
    if (virtualKeyCode == 0) return;

    // If holdDuration > 0, keep the key down for that duration
    if (holdDuration > 0.01f) {
        INPUT down = {0};
        down.type = INPUT_KEYBOARD;
        down.ki.wVk = static_cast<WORD>(virtualKeyCode);
        SendInput(1, &down, sizeof(INPUT));

        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<uint32_t>(holdDuration * 1000.0f)));

        INPUT up = {0};
        up.type = INPUT_KEYBOARD;
        up.ki.wVk = static_cast<WORD>(virtualKeyCode);
        up.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &up, sizeof(INPUT));
    }
    else {
        PressKey(static_cast<WORD>(virtualKeyCode));
    }
}

}} // namespace AutoReflex::Game
