// AutoReflex - AnimationLock
// Blocks rule evaluation briefly after a key press (WaitAfterPressMs).

#pragma once

#include <chrono>

namespace AutoReflex {
namespace Core {

class AnimationLock {
public:
    bool IsLocked() const {
        return std::chrono::steady_clock::now() < m_LockedUntil;
    }

    void Engage(float waitAfterPressMs) {
        if (waitAfterPressMs <= 0.0f) return;
        const auto lockedUntil = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(static_cast<int>(waitAfterPressMs));
        if (lockedUntil > m_LockedUntil) {
            m_LockedUntil = lockedUntil;
        }
    }

    void Reset() { m_LockedUntil = std::chrono::steady_clock::time_point{}; }

private:
    std::chrono::steady_clock::time_point m_LockedUntil{};
};

} // namespace Core
} // namespace AutoReflex
