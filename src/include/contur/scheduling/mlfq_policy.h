/// @file mlfq_policy.h
/// @brief Multilevel Feedback Queue scheduling policy.

#pragma once

#include <cstddef>
#include <vector>

#include "contur/scheduling/i_scheduling_policy.h"

namespace contur {

    /// @brief Multilevel Feedback Queue (MLFQ) scheduling policy.
    ///
    /// Classifies processes across multiple priority levels with distinct
    /// time slices and adapts scheduling behavior from runtime feedback.
    class MlfqPolicy final : public ISchedulingPolicy
    {
        public:
        /// @brief Constructs MLFQ policy.
        /// @param levelTimeSlices Time slice per level (highest to lowest).
        explicit MlfqPolicy(std::vector<std::size_t> levelTimeSlices = {1, 2, 4});

        /// @brief Policy name.
        [[nodiscard]] std::string_view name() const noexcept override;

        /// @brief Selects next process according to MLFQ level ordering.
        [[nodiscard]] ProcessId
        selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const override;

        /// @brief Returns true when running process should be preempted.
        [[nodiscard]] bool shouldPreempt(
            const SchedulingProcessSnapshot &running, const SchedulingProcessSnapshot &candidate, const IClock &clock
        ) const override;

        /// @brief Configured level time slices.
        [[nodiscard]] const std::vector<std::size_t> &levelTimeSlices() const noexcept;

        private:
        std::vector<std::size_t> levelTimeSlices_;
    };

} // namespace contur
