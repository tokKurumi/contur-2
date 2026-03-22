/// @file fcfs_policy.h
/// @brief First Come First Served scheduling policy.

#pragma once

#include "contur/scheduling/i_scheduling_policy.h"

namespace contur {

    /// @brief First Come First Served (FCFS) scheduling policy.
    ///
    /// Selects the ready process with the earliest arrival/queue order.
    /// FCFS is non-preemptive.
    class FcfsPolicy final : public ISchedulingPolicy
    {
        public:
        /// @brief Policy name.
        [[nodiscard]] std::string_view name() const noexcept override;

        /// @brief Selects the next process in FCFS order.
        [[nodiscard]] ProcessId selectNext(
            const std::vector<std::reference_wrapper<const PCB>> &readyQueue, const IClock &clock
        ) const override;

        /// @brief FCFS never preempts the currently running process.
        [[nodiscard]] bool shouldPreempt(const PCB &running, const PCB &candidate, const IClock &clock) const override;
    };

} // namespace contur
