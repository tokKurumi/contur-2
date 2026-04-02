/// @file round_robin_policy.h
/// @brief Round Robin scheduling policy.

#pragma once

#include <cstddef>

#include "contur/scheduling/i_scheduling_policy.h"

namespace contur {

    /// @brief Round Robin scheduling policy.
    ///
    /// Uses fixed-size time slices and preempts when the running process
    /// consumes its configured budget.
    class RoundRobinPolicy final : public ISchedulingPolicy
    {
        public:
        /// @brief Constructs Round Robin policy.
        /// @param timeSlice Maximum ticks a process can run before preemption.
        explicit RoundRobinPolicy(std::size_t timeSlice);

        /// @brief Policy name.
        [[nodiscard]] std::string_view name() const noexcept override;

        /// @brief Selects the next process in round-robin queue order.
        [[nodiscard]] ProcessId
        selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const override;

        /// @brief Returns true if running process exhausted its slice.
        [[nodiscard]] bool shouldPreempt(
            const SchedulingProcessSnapshot &running, const SchedulingProcessSnapshot &candidate, const IClock &clock
        ) const override;

        /// @brief Configured time slice.
        [[nodiscard]] std::size_t timeSlice() const noexcept;

        private:
        std::size_t timeSlice_;
    };

} // namespace contur
