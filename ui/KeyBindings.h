// AutoReflex - shared virtual-key labels for settings UI.

#pragma once

#include <Windows.h>

#include <cstdint>

namespace AutoReflex {
namespace UI {
namespace KeyBindings {

inline constexpr const char* kLabels[] = {
    "none","a","b","c","d","e","f","g","h","i","j","k","l","m",
    "n","o","p","q","r","s","t","u","v","w","x","y","z",
    "0","1","2","3","4","5","6","7","8","9",
    "f1","f2","f3","f4","f5","f6","f7","f8","f9","f10","f11","f12",
    "lbutton","rbutton","mbutton","xbutton1","xbutton2"
};

inline constexpr uint16_t kValues[] = {
    0,'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '0','1','2','3','4','5','6','7','8','9',
    VK_F1,VK_F2,VK_F3,VK_F4,VK_F5,VK_F6,VK_F7,VK_F8,VK_F9,VK_F10,VK_F11,VK_F12,
    VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2
};

inline constexpr int kCount = static_cast<int>(sizeof(kValues) / sizeof(kValues[0]));

inline const char* LabelForVirtualKey(uint16_t virtualKey) {
    for (int i = 0; i < kCount; ++i) {
        if (kValues[i] == virtualKey) return kLabels[i];
    }
    return "none";
}

} // namespace KeyBindings
} // namespace UI
} // namespace AutoReflex
