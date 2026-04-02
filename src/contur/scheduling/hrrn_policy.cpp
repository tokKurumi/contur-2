/// @file hrrn_policy.cpp
/// @brief HRRN scheduling policy implementation.

#include "contur/scheduling/hrrn_policy.h"

#include <algorithm>

#include "contur/core/clock.h"

namespace contur {

    namespace {
        [[nodiscard]] double responseRatio(const SchedulingProcessSnapshot &process) noexcept
        {
            double service = static_cast<double>(process.estimatedBurst == 0 ? 1 : process.estimatedBurst);
            double wait = static_cast<double>(process.totalWaitTime);
            return (wait + service) / service;
        }
    } // namespace

    std::string_view HrrnPolicy::name() const noexcept
    {
        return "HRRN";
    }

    ProcessId
    HrrnPolicy::selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const
    {
        (void)clock;
        if (readyQueue.empty())
        {
            return INVALID_PID;
        }

        auto selected = std::max_element(readyQueue.begin(), readyQueue.end(), [](const auto &a, const auto &b) {
            double lhs = responseRatio(a);
            double rhs = responseRatio(b);
            if (lhs != rhs)
            {
                return lhs < rhs;
            }
            return a.pid > b.pid;
        });
        return selected->pid;
    }

    bool HrrnPolicy::shouldPreempt(
        const SchedulingProcessSnapshot &running, const SchedulingProcessSnapshot &candidate, const IClock &clock
    ) const
    {
        (void)running;
        (void)candidate;
        (void)clock;
        return false;
    }

} // namespace contur
