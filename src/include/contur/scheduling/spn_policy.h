/// @file spn_policy.h
/// @brief Shortest Process Next scheduling policy.

#pragma once

#include "contur/scheduling/i_scheduling_policy.h"

namespace contur {

    /// @brief Shortest Process Next (SPN) scheduling policy.
    ///
    /// Selects the ready process with the smallest predicted CPU burst.
    /// SPN is non-preemptive.
    class SpnPolicy final : public ISchedulingPolicy
    {
        public:
        /// @brief Policy name.
        [[nodiscard]] std::string_view name() const noexcept override;

        /// @brief Selects process with minimal estimated burst.
        [[nodiscard]] ProcessId
        selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const override;

        /// @brief SPN does not preempt once a process is running.
        [[nodiscard]] bool shouldPreempt(
            const SchedulingProcessSnapshot &running, const SchedulingProcessSnapshot &candidate, const IClock &clock
        ) const override;
    };

} // namespace contur
