/// @file hrrn_policy.cpp
/// @brief HRRN scheduling policy implementation.

#include "contur/scheduling/hrrn_policy.h"

#include <algorithm>

#include "contur/core/clock.h"

#include "contur/process/pcb.h"

namespace contur {

    namespace {
        [[nodiscard]] double responseRatio(const PCB &pcb) noexcept
        {
            double service = static_cast<double>(pcb.timing().estimatedBurst == 0 ? 1 : pcb.timing().estimatedBurst);
            double wait = static_cast<double>(pcb.timing().totalWaitTime);
            return (wait + service) / service;
        }
    } // namespace

    std::string_view HrrnPolicy::name() const noexcept
    {
        return "HRRN";
    }

    ProcessId
    HrrnPolicy::selectNext(const std::vector<std::reference_wrapper<const PCB>> &readyQueue, const IClock &clock) const
    {
        (void)clock;
        if (readyQueue.empty())
        {
            return INVALID_PID;
        }

        auto selected = std::max_element(readyQueue.begin(), readyQueue.end(), [](const auto &a, const auto &b) {
            double lhs = responseRatio(a.get());
            double rhs = responseRatio(b.get());
            if (lhs != rhs)
            {
                return lhs < rhs;
            }
            return a.get().id() > b.get().id();
        });
        return selected->get().id();
    }

    bool HrrnPolicy::shouldPreempt(const PCB &running, const PCB &candidate, const IClock &clock) const
    {
        (void)running;
        (void)candidate;
        (void)clock;
        return false;
    }

} // namespace contur
