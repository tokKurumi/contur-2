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
        /// @note In multi-lane deployments prefer blockProcess(pid, tick) to avoid
        ///       ambiguity when several lanes have a running process simultaneously.
        [[nodiscard]] virtual Result<void> blockRunning(Tick currentTick) = 0;

        /// @brief Moves a specific running or ready process to the blocked queue.
        /// @param pid Process identifier to block.
        /// @param currentTick Simulation tick for timing metadata.
        /// @return Ok on success; NotFound if pid is unknown; InvalidState on bad transition.
        [[nodiscard]] virtual Result<void> blockProcess(ProcessId pid, Tick currentTick) = 0;

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

        /// @brief Configures scheduler lane count used for per-core ready queues.
        /// @param laneCount Number of scheduler lanes (must be >= 1).
        /// @return Ok on success; InvalidArgument for zero; InvalidState if scheduler is not empty.
        [[nodiscard]] virtual Result<void> configureLanes(std::size_t laneCount) = 0;

        /// @brief Returns number of configured scheduler lanes.
        [[nodiscard]] virtual std::size_t laneCount() const noexcept = 0;

        /// @brief Enqueues process into a specific ready lane.
        /// @param pcb Process control block to add.
        /// @param laneIndex Target lane index.
        /// @param currentTick Simulation tick used for queue timing metadata.
        /// @return Ok on success; InvalidArgument for bad lane/pid; InvalidState for invalid transition.
        [[nodiscard]] virtual Result<void> enqueueToLane(PCB &pcb, std::size_t laneIndex, Tick currentTick) = 0;

        /// @brief Selects/runs next process on a specific lane.
        /// @param laneIndex Target lane index.
        /// @param clock Simulation clock for policy decisions.
        /// @return Selected process id, or NotFound/InvalidState.
        [[nodiscard]] virtual Result<ProcessId> selectNextForLane(std::size_t laneIndex, const IClock &clock) = 0;

        /// @brief Steals one ready process from another lane and schedules it on thief lane.
        ///
        /// This is **scheduler-level** (intra-dispatcher) work stealing: it moves a
        /// simulated process between per-core ready queues inside a single dispatcher.
        /// It is independent of runtime-level (inter-dispatcher) work stealing performed
        /// by DispatcherPool, which moves whole dispatcher lanes between host threads.
        ///
        /// @param thiefLane Lane that steals work.
        /// @param clock Simulation clock for selection and preemption checks.
        /// @return Selected process id on thief lane, or NotFound/InvalidState.
        [[nodiscard]] virtual Result<ProcessId> stealNextForLane(std::size_t thiefLane, const IClock &clock) = 0;

        /// @brief Returns per-lane ready queue snapshots.
        /// @return One ready queue snapshot per lane.
        [[nodiscard]] virtual std::vector<std::vector<ProcessId>> getPerLaneQueueSnapshot() const = 0;

        /// @brief Returns currently running process IDs across scheduler lanes.
        /// @return Running process identifiers; empty when all lanes are idle.
        [[nodiscard]] virtual std::vector<ProcessId> runningProcesses() const = 0;

        /// @brief Replaces scheduling policy at runtime.
        /// @param policy New scheduling policy instance.
        /// @return Ok on success; InvalidState if policy is null.
        [[nodiscard]] virtual Result<void> setPolicy(std::unique_ptr<ISchedulingPolicy> policy) = 0;
    };

} // namespace contur
