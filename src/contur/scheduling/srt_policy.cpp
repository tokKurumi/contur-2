/// @file srt_policy.cpp
/// @brief SRT scheduling policy implementation.

#include "contur/scheduling/srt_policy.h"

#include <algorithm>

#include "contur/core/clock.h"

namespace contur {

    namespace {
        [[nodiscard]] Tick remaining(const SchedulingProcessSnapshot &process) noexcept
        {
            if (process.remainingBurst > 0)
            {
                return process.remainingBurst;
            }
            if (process.estimatedBurst > 0)
            {
                return process.estimatedBurst;
            }
            return 1;
        }
    } // namespace

    std::string_view SrtPolicy::name() const noexcept
    {
        return "SRT";
    }

    ProcessId SrtPolicy::selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const
    {
        (void)clock;
        if (readyQueue.empty())
        {
            return INVALID_PID;
        }

        auto selected = std::min_element(readyQueue.begin(), readyQueue.end(), [](const auto &a, const auto &b) {
            Tick lhs = remaining(a);
            Tick rhs = remaining(b);
            if (lhs != rhs)
            {
                return lhs < rhs;
            }
            return a.pid < b.pid;
        });
        return selected->pid;
    }

    bool SrtPolicy::shouldPreempt(
        const SchedulingProcessSnapshot &running, const SchedulingProcessSnapshot &candidate, const IClock &clock
    ) const
    {
        (void)clock;
        return remaining(candidate) < remaining(running);
    }

} // namespace contur
