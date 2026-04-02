/// @file priority_policy.h
/// @brief Dynamic priority scheduling policy.

#pragma once

#include "contur/scheduling/i_scheduling_policy.h"

namespace contur {

    /// @brief Dynamic-priority scheduling policy.
    ///
    /// Selects process with best effective priority and supports preemption
    /// when a higher-priority candidate becomes ready.
    class PriorityPolicy final : public ISchedulingPolicy
    {
        public:
        /// @brief Policy name.
        [[nodiscard]] std::string_view name() const noexcept override;

        /// @brief Selects process with highest effective priority.
        [[nodiscard]] ProcessId
        selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const override;

        /// @brief Preempts when candidate priority outranks running process.
        [[nodiscard]] bool shouldPreempt(
            const SchedulingProcessSnapshot &running, const SchedulingProcessSnapshot &candidate, const IClock &clock
        ) const override;
    };

} // namespace contur
