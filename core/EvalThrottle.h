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
    /**
     * Determines whether rule evaluation should run for the current tick.
     *
     * @param evaluationIntervalMs Minimum time between accepted ticks; <= 0 disables throttling.
     * @returns True when evaluation is permitted now; otherwise false.
     */
    bool DetermineWhetherEvaluationShouldRunNow(int evaluationIntervalMs) {
        if (evaluationIntervalMs <= 0) return true;
        const auto now = std::chrono::steady_clock::now();
        if (now < m_NextEval) return false;
        m_NextEval = now + std::chrono::milliseconds(evaluationIntervalMs);
        return true;
    }

    /** Resets the throttle so the next call is accepted immediately. */
    void Reset() { m_NextEval = std::chrono::steady_clock::time_point{}; }

private:
    std::chrono::steady_clock::time_point m_NextEval{};
};

} // namespace Core
} // namespace AutoReflex
