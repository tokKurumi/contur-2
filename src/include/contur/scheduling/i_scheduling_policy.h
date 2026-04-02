/// @file i_scheduling_policy.h
/// @brief ISchedulingPolicy interface for pluggable scheduling algorithms.

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "contur/core/types.h"

#include "contur/process/priority.h"

namespace contur {

    class IClock;

    /// @brief Immutable process view consumed by scheduling policies.
    ///
    /// Policies receive this value-type snapshot to avoid direct access to
    /// mutable/shared PCB state.
    struct SchedulingProcessSnapshot
    {
        ProcessId pid = INVALID_PID;
        Tick arrivalTime = 0;
        Tick lastStateChange = 0;
        Tick estimatedBurst = 0;
        Tick remainingBurst = 0;
        Tick totalWaitTime = 0;
        PriorityLevel effectivePriority = PriorityLevel::Normal;
        std::int32_t nice = NICE_DEFAULT;
    };

    /// @brief Strategy interface for scheduling algorithms.
    class ISchedulingPolicy
    {
        public:
        virtual ~ISchedulingPolicy() = default;

        /// @brief Human-readable policy name.
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;

        /// @brief Selects the next process ID from the ready queue.
        /// @param readyQueue Immutable snapshot of ready processes.
        /// @param clock Simulation clock.
        /// @return Selected process ID, or INVALID_PID if queue is empty.
        [[nodiscard]] virtual ProcessId
        selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const = 0;

        /// @brief Decides whether the current running process should be preempted.
        /// @param running Currently running process.
        /// @param candidate Candidate ready process chosen by selectNext().
        /// @param clock Simulation clock.
        [[nodiscard]] virtual bool shouldPreempt(
            const SchedulingProcessSnapshot &running, const SchedulingProcessSnapshot &candidate, const IClock &clock
        ) const = 0;
    };

} // namespace contur
