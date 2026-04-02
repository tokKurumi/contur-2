/// @file priority_policy.cpp
/// @brief Dynamic priority scheduling policy implementation.

#include "contur/scheduling/priority_policy.h"

#include <algorithm>

#include "contur/core/clock.h"

namespace contur {

    namespace {
        [[nodiscard]] bool
        betterPriority(const SchedulingProcessSnapshot &lhs, const SchedulingProcessSnapshot &rhs) noexcept
        {
            if (lhs.effectivePriority != rhs.effectivePriority)
            {
                return static_cast<std::int8_t>(lhs.effectivePriority) <
                       static_cast<std::int8_t>(rhs.effectivePriority);
            }
            if (lhs.nice != rhs.nice)
            {
                return lhs.nice < rhs.nice;
            }
            return lhs.pid < rhs.pid;
        }
    } // namespace

    std::string_view PriorityPolicy::name() const noexcept
    {
        return "Priority";
    }

    ProcessId
    PriorityPolicy::selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const
    {
        (void)clock;
        if (readyQueue.empty())
        {
            return INVALID_PID;
        }

        auto selected = std::min_element(readyQueue.begin(), readyQueue.end(), [](const auto &a, const auto &b) {
            return betterPriority(a, b);
        });
        return selected->pid;
    }

    bool PriorityPolicy::shouldPreempt(
        const SchedulingProcessSnapshot &running, const SchedulingProcessSnapshot &candidate, const IClock &clock
    ) const
    {
        (void)clock;
        return betterPriority(candidate, running);
    }

} // namespace contur
