/// @file i_scheduler.h
/// @brief IScheduler interface.

#pragma once

#include <memory>
#include <vector>

#include "contur/core/error.h"
#include "contur/core/types.h"

namespace contur {

    class IClock;
    class PCB;
    class ISchedulingPolicy;

    /// @brief Scheduler abstraction managing process state queues.
    ///
    /// Coordinates ready/blocked/running process sets and delegates process
    /// ordering decisions to the active ISchedulingPolicy implementation.
    class IScheduler
    {
        public:
        virtual ~IScheduler() = default;

        /// @brief Enqueues a process into the ready queue.
        /// @param pcb Process control block to add.
        /// @param currentTick Simulation tick used for queue timing metadata.
        /// @return Ok on success; error if enqueueing is invalid for current state.
        [[nodiscard]] virtual Result<void> enqueue(PCB &pcb, Tick currentTick) = 0;

        /// @brief Removes a process from scheduler ownership/queues.
        /// @param pid Identifier of the process to remove.
        /// @return Ok on success; error if pid is unknown.
        [[nodiscard]] virtual Result<void> dequeue(ProcessId pid) = 0;

        /// @brief Selects the process to run next.
        /// @param clock Simulation clock for time-aware policy decisions.
        /// @return Selected process identifier, or error if no runnable process exists.
        [[nodiscard]] virtual Result<ProcessId> selectNext(const IClock &clock) = 0;

        /// @brief Moves current running process to blocked queue.
        /// @param currentTick Simulation tick for blocked queue timing metadata.
        /// @return Ok on success; error if no process is currently running.
        [[nodiscard]] virtual Result<void> blockRunning(Tick currentTick) = 0;

        /// @brief Moves a blocked process back to ready queue.
        /// @param pid Identifier of the blocked process.
        /// @param currentTick Simulation tick used when re-enqueueing.
        /// @return Ok on success; error if pid is not blocked.
        [[nodiscard]] virtual Result<void> unblock(ProcessId pid, Tick currentTick) = 0;

        /// @brief Marks process as terminated and removes it from active queues.
        /// @param pid Identifier of the process to terminate.
        /// @param currentTick Simulation tick used for final accounting.
        /// @return Ok on success; error if pid is unknown.
        [[nodiscard]] virtual Result<void> terminate(ProcessId pid, Tick currentTick) = 0;

        /// @brief Returns ready queue PID snapshot.
        /// @return Ordered list of process identifiers currently in the ready queue.
        [[nodiscard]] virtual std::vector<ProcessId> getQueueSnapshot() const = 0;

        /// @brief Returns blocked queue PID snapshot.
        /// @return Ordered list of process identifiers currently in the blocked queue.
        [[nodiscard]] virtual std::vector<ProcessId> getBlockedSnapshot() const = 0;

        /// @brief Returns currently running process, or INVALID_PID.
        /// @return Running process identifier, or INVALID_PID if CPU is idle.
        [[nodiscard]] virtual ProcessId runningProcess() const noexcept = 0;

        /// @brief Replaces scheduling policy at runtime.
        /// @param policy New scheduling policy instance.
        /// @return Ok on success; error if policy is null or unsupported.
        [[nodiscard]] virtual Result<void> setPolicy(std::unique_ptr<ISchedulingPolicy> policy) = 0;
    };

} // namespace contur
