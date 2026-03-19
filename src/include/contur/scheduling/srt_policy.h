/// @file srt_policy.h
/// @brief Shortest Remaining Time scheduling policy.

#pragma once

#include "contur/scheduling/i_scheduling_policy.h"

namespace contur {

    class SrtPolicy final : public ISchedulingPolicy
    {
        public:
        [[nodiscard]] std::string_view name() const noexcept override;
        [[nodiscard]] ProcessId selectNext(
            const std::vector<std::reference_wrapper<const PCB>> &readyQueue, const IClock &clock
        ) const override;
        [[nodiscard]] bool shouldPreempt(const PCB &running, const PCB &candidate, const IClock &clock) const override;
    };

} // namespace contur
