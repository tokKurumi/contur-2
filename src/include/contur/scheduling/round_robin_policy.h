/// @file round_robin_policy.h
/// @brief Round Robin scheduling policy.

#pragma once

#include <cstddef>

#include "contur/scheduling/i_scheduling_policy.h"

namespace contur {

    class RoundRobinPolicy final : public ISchedulingPolicy
    {
        public:
        explicit RoundRobinPolicy(std::size_t timeSlice);

        [[nodiscard]] std::string_view name() const noexcept override;
        [[nodiscard]] ProcessId selectNext(
            const std::vector<std::reference_wrapper<const PCB>> &readyQueue, const IClock &clock
        ) const override;
        [[nodiscard]] bool shouldPreempt(const PCB &running, const PCB &candidate, const IClock &clock) const override;

        [[nodiscard]] std::size_t timeSlice() const noexcept;

        private:
        std::size_t timeSlice_;
    };

} // namespace contur
