// AutoReflex - EvalThrottle
// Frame-rate independent rule evaluation pacing.
//
// DrawUI() runs at the host's render rate (60-240 Hz). Rule evaluation does
// not need to keep up with that — cooldowns are measured in 100s of ms and
// human reaction time dwarfs any sub-frame jitter. ShouldEvaluate() returns
// true at most once per `intervalMs` and schedules the next slot.

#pragma once

#include <chrono>

namespace AutoReflex {
namespace Core {

class EvalThrottle {
public:
    // Returns true when at least intervalMs has passed since the last accepted
    // tick. On true, the next slot is scheduled `intervalMs` later.
    // intervalMs <= 0 disables the throttle (always returns true).
    bool ShouldEvaluate(int intervalMs) {
        if (intervalMs <= 0) return true;
        const auto now = std::chrono::steady_clock::now();
        if (now < m_NextEval) return false;
        m_NextEval = now + std::chrono::milliseconds(intervalMs);
        return true;
    }

    void Reset() { m_NextEval = std::chrono::steady_clock::time_point{}; }

private:
    std::chrono::steady_clock::time_point m_NextEval{};
};

} // namespace Core
} // namespace AutoReflex
