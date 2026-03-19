/// @file i_scheduling_policy.h
/// @brief ISchedulingPolicy interface for pluggable scheduling algorithms.

#pragma once

#include <functional>
#include <string_view>
#include <vector>

#include "contur/core/types.h"

namespace contur {

    class IClock;
    class PCB;

    /// @brief Strategy interface for scheduling algorithms.
    class ISchedulingPolicy
    {
        public:
        virtual ~ISchedulingPolicy() = default;

        /// @brief Human-readable policy name.
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;

        /// @brief Selects the next process ID from the ready queue.
        /// @param readyQueue Non-owning references to ready PCBs.
        /// @param clock Simulation clock.
        /// @return Selected process ID, or INVALID_PID if queue is empty.
        [[nodiscard]] virtual ProcessId
        selectNext(const std::vector<std::reference_wrapper<const PCB>> &readyQueue, const IClock &clock) const = 0;

        /// @brief Decides whether the current running process should be preempted.
        /// @param running Currently running process.
        /// @param candidate Candidate ready process chosen by selectNext().
        /// @param clock Simulation clock.
        [[nodiscard]] virtual bool
        shouldPreempt(const PCB &running, const PCB &candidate, const IClock &clock) const = 0;
    };

} // namespace contur
