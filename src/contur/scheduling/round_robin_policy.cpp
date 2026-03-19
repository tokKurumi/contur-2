/// @file round_robin_policy.cpp
/// @brief Round Robin scheduling policy implementation.

#include "contur/scheduling/round_robin_policy.h"

#include <algorithm>

#include "contur/core/clock.h"

#include "contur/process/pcb.h"

namespace contur {

    RoundRobinPolicy::RoundRobinPolicy(std::size_t timeSlice)
        : timeSlice_(timeSlice == 0 ? 1 : timeSlice)
    {}

    std::string_view RoundRobinPolicy::name() const noexcept
    {
        return "RoundRobin";
    }

    ProcessId RoundRobinPolicy::selectNext(
        const std::vector<std::reference_wrapper<const PCB>> &readyQueue, const IClock &clock
    ) const
    {
        (void)clock;
        if (readyQueue.empty())
        {
            return INVALID_PID;
        }

        auto selected = std::min_element(readyQueue.begin(), readyQueue.end(), [](const auto &a, const auto &b) {
            if (a.get().timing().lastStateChange != b.get().timing().lastStateChange)
            {
                return a.get().timing().lastStateChange < b.get().timing().lastStateChange;
            }
            return a.get().id() < b.get().id();
        });
        return selected->get().id();
    }

    bool RoundRobinPolicy::shouldPreempt(const PCB &running, const PCB &candidate, const IClock &clock) const
    {
        (void)candidate;
        Tick elapsed = clock.now() - running.timing().lastStateChange;
        return elapsed >= timeSlice_;
    }

    std::size_t RoundRobinPolicy::timeSlice() const noexcept
    {
        return timeSlice_;
    }

} // namespace contur
