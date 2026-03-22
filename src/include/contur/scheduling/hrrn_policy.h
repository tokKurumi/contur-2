/// @file hrrn_policy.h
/// @brief Highest Response Ratio Next scheduling policy.

#pragma once

#include "contur/scheduling/i_scheduling_policy.h"

namespace contur {

    /// @brief Highest Response Ratio Next (HRRN) scheduling policy.
    ///
    /// Selects process maximizing response ratio:
    /// (waiting_time + service_time) / service_time.
    class HrrnPolicy final : public ISchedulingPolicy
    {
        public:
        /// @brief Policy name.
        [[nodiscard]] std::string_view name() const noexcept override;

        /// @brief Selects process with highest response ratio.
        [[nodiscard]] ProcessId selectNext(
            const std::vector<std::reference_wrapper<const PCB>> &readyQueue, const IClock &clock
        ) const override;

        /// @brief HRRN is non-preemptive once process is running.
        [[nodiscard]] bool shouldPreempt(const PCB &running, const PCB &candidate, const IClock &clock) const override;
    };

} // namespace contur
