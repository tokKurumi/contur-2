/// @file srt_policy.h
/// @brief Shortest Remaining Time scheduling policy.

#pragma once

#include "contur/scheduling/i_scheduling_policy.h"

namespace contur {

    /// @brief Shortest Remaining Time (SRT) scheduling policy.
    ///
    /// Selects process with minimal remaining estimated CPU burst and
    /// allows preemption by shorter candidates.
    class SrtPolicy final : public ISchedulingPolicy
    {
        public:
        /// @brief Policy name.
        [[nodiscard]] std::string_view name() const noexcept override;

        /// @brief Selects process with the shortest remaining burst.
        [[nodiscard]] ProcessId
        selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const override;

        /// @brief Preempts when candidate has smaller remaining burst.
        [[nodiscard]] bool shouldPreempt(
            const SchedulingProcessSnapshot &running, const SchedulingProcessSnapshot &candidate, const IClock &clock
        ) const override;
    };

} // namespace contur
