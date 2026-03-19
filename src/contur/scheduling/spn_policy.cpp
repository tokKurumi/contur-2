/// @file spn_policy.cpp
/// @brief SPN scheduling policy implementation.

#include "contur/scheduling/spn_policy.h"

#include <algorithm>

#include "contur/core/clock.h"

#include "contur/process/pcb.h"

namespace contur {

    namespace {
        [[nodiscard]] Tick serviceTime(const PCB &pcb) noexcept
        {
            Tick estimate = pcb.timing().estimatedBurst;
            return estimate == 0 ? 1 : estimate;
        }
    } // namespace

    std::string_view SpnPolicy::name() const noexcept
    {
        return "SPN";
    }

    ProcessId
    SpnPolicy::selectNext(const std::vector<std::reference_wrapper<const PCB>> &readyQueue, const IClock &clock) const
    {
        (void)clock;
        if (readyQueue.empty())
        {
            return INVALID_PID;
        }

        auto selected = std::min_element(readyQueue.begin(), readyQueue.end(), [](const auto &a, const auto &b) {
            Tick lhs = serviceTime(a.get());
            Tick rhs = serviceTime(b.get());
            if (lhs != rhs)
            {
                return lhs < rhs;
            }
            return a.get().id() < b.get().id();
        });
        return selected->get().id();
    }

    bool SpnPolicy::shouldPreempt(const PCB &running, const PCB &candidate, const IClock &clock) const
    {
        (void)running;
        (void)candidate;
        (void)clock;
        return false;
    }

} // namespace contur
