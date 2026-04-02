/// @file fcfs_policy.cpp
/// @brief FCFS scheduling policy implementation.

#include "contur/scheduling/fcfs_policy.h"

#include <algorithm>

#include "contur/core/clock.h"

namespace contur {

    std::string_view FcfsPolicy::name() const noexcept
    {
        return "FCFS";
    }

    ProcessId
    FcfsPolicy::selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const
    {
        (void)clock;
        if (readyQueue.empty())
        {
            return INVALID_PID;
        }

        auto selected = std::min_element(readyQueue.begin(), readyQueue.end(), [](const auto &a, const auto &b) {
            if (a.arrivalTime != b.arrivalTime)
            {
                return a.arrivalTime < b.arrivalTime;
            }
            return a.pid < b.pid;
        });

        return selected->pid;
    }

    bool FcfsPolicy::shouldPreempt(
        const SchedulingProcessSnapshot &running, const SchedulingProcessSnapshot &candidate, const IClock &clock
    ) const
    {
        (void)running;
        (void)candidate;
        (void)clock;
        return false;
    }

} // namespace contur
