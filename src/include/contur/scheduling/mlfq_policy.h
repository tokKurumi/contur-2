/// @file mlfq_policy.h
/// @brief Multilevel Feedback Queue scheduling policy.

#pragma once

#include <cstddef>
#include <vector>

#include "contur/scheduling/i_scheduling_policy.h"

namespace contur {

    class MlfqPolicy final : public ISchedulingPolicy
    {
        public:
        explicit MlfqPolicy(std::vector<std::size_t> levelTimeSlices = {1, 2, 4});

        [[nodiscard]] std::string_view name() const noexcept override;
        [[nodiscard]] ProcessId selectNext(
            const std::vector<std::reference_wrapper<const PCB>> &readyQueue, const IClock &clock
        ) const override;
        [[nodiscard]] bool shouldPreempt(const PCB &running, const PCB &candidate, const IClock &clock) const override;

        [[nodiscard]] const std::vector<std::size_t> &levelTimeSlices() const noexcept;

        private:
        std::vector<std::size_t> levelTimeSlices_;
    };

} // namespace contur
