/// @file round_robin_policy.cpp
/// @brief Round Robin scheduling policy implementation.

#include "contur/scheduling/round_robin_policy.h"

#include <algorithm>

#include "contur/core/clock.h"

namespace contur {

    RoundRobinPolicy::RoundRobinPolicy(std::size_t timeSlice)
        : timeSlice_(timeSlice == 0 ? 1 : timeSlice)
    {}

    std::string_view RoundRobinPolicy::name() const noexcept
    {
        return "RoundRobin";
    }

    ProcessId
    RoundRobinPolicy::selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const
    {
        (void)clock;
        if (readyQueue.empty())
        {
            return INVALID_PID;
        }

        auto selected = std::min_element(readyQueue.begin(), readyQueue.end(), [](const auto &a, const auto &b) {
            if (a.lastStateChange != b.lastStateChange)
            {
                return a.lastStateChange < b.lastStateChange;
            }
            return a.pid < b.pid;
        });
        return selected->pid;
    }

    bool RoundRobinPolicy::shouldPreempt(
        const SchedulingProcessSnapshot &running, const SchedulingProcessSnapshot &candidate, const IClock &clock
    ) const
    {
        (void)candidate;
        Tick elapsed = clock.now() - running.lastStateChange;
        return elapsed >= timeSlice_;
    }

    std::size_t RoundRobinPolicy::timeSlice() const noexcept
    {
        return timeSlice_;
    }

} // namespace contur
