/// @file fcfs_policy.cpp
/// @brief FCFS scheduling policy implementation.

#include "contur/scheduling/fcfs_policy.h"

#include <algorithm>

#include "contur/core/clock.h"

#include "contur/process/pcb.h"

namespace contur {

    std::string_view FcfsPolicy::name() const noexcept
    {
        return "FCFS";
    }

    ProcessId
    FcfsPolicy::selectNext(const std::vector<std::reference_wrapper<const PCB>> &readyQueue, const IClock &clock) const
    {
        (void)clock;
        if (readyQueue.empty())
        {
            return INVALID_PID;
        }

        auto selected = std::min_element(readyQueue.begin(), readyQueue.end(), [](const auto &a, const auto &b) {
            if (a.get().timing().arrivalTime != b.get().timing().arrivalTime)
            {
                return a.get().timing().arrivalTime < b.get().timing().arrivalTime;
            }
            return a.get().id() < b.get().id();
        });

        return selected->get().id();
    }

    bool FcfsPolicy::shouldPreempt(const PCB &running, const PCB &candidate, const IClock &clock) const
    {
        (void)running;
        (void)candidate;
        (void)clock;
        return false;
    }

} // namespace contur
