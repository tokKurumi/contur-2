/// @file spn_policy.cpp
/// @brief SPN scheduling policy implementation.

#include "contur/scheduling/spn_policy.h"

#include <algorithm>

#include "contur/core/clock.h"

namespace contur {

    namespace {
        [[nodiscard]] Tick serviceTime(const SchedulingProcessSnapshot &process) noexcept
        {
            Tick estimate = process.estimatedBurst;
            return estimate == 0 ? 1 : estimate;
        }
    } // namespace

    std::string_view SpnPolicy::name() const noexcept
    {
        return "SPN";
    }

    ProcessId SpnPolicy::selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const
    {
        (void)clock;
        if (readyQueue.empty())
        {
            return INVALID_PID;
        }

        auto selected = std::min_element(readyQueue.begin(), readyQueue.end(), [](const auto &a, const auto &b) {
            Tick lhs = serviceTime(a);
            Tick rhs = serviceTime(b);
            if (lhs != rhs)
            {
                return lhs < rhs;
            }
            return a.pid < b.pid;
        });
        return selected->pid;
    }

    bool SpnPolicy::shouldPreempt(
        const SchedulingProcessSnapshot &running, const SchedulingProcessSnapshot &candidate, const IClock &clock
    ) const
    {
        (void)running;
        (void)candidate;
        (void)clock;
        return false;
    }

} // namespace contur
