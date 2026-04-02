/// @file scheduler.h
/// @brief Scheduler implementation hosting a pluggable policy.

#pragma once

#include <memory>

#include "contur/scheduling/i_scheduler.h"

namespace contur {

    class ITracer;

    /// @brief Scheduler host that manages process queues and policy decisions.
    ///
    /// Scheduler owns queue state and delegates ordering/preemption logic
    /// to the active ISchedulingPolicy implementation.
    class Scheduler final : public IScheduler
    {
        public:
        /// @brief Constructs scheduler with initial policy.
        /// @param policy Initial scheduling policy. Null policy leaves scheduler unconfigured.
        explicit Scheduler(std::unique_ptr<ISchedulingPolicy> policy, ITracer &tracer);

        /// @brief Destroys scheduler.
        ~Scheduler() override;

        /// @brief Copy construction is disabled.
        Scheduler(const Scheduler &) = delete;

        /// @brief Copy assignment is disabled.
        Scheduler &operator=(const Scheduler &) = delete;
        /// @brief Move-constructs scheduler state.
        Scheduler(Scheduler &&) noexcept;

        /// @brief Move-assigns scheduler state.
        Scheduler &operator=(Scheduler &&) noexcept;

        /// @brief Enqueues process into ready queue.
        [[nodiscard]] Result<void> enqueue(PCB &pcb, Tick currentTick) override;

        /// @brief Removes process from scheduler tracking.
        [[nodiscard]] Result<void> dequeue(ProcessId pid) override;

        /// @brief Selects next process according to active policy.
        [[nodiscard]] Result<ProcessId> selectNext(const IClock &clock) override;

        /// @brief Moves running process to blocked state.
        [[nodiscard]] Result<void> blockRunning(Tick currentTick) override;

        /// @brief Moves a specific process to blocked state.
        [[nodiscard]] Result<void> blockProcess(ProcessId pid, Tick currentTick) override;

        /// @brief Moves blocked process back to ready queue.
        [[nodiscard]] Result<void> unblock(ProcessId pid, Tick currentTick) override;

        /// @brief Terminates process and removes it from scheduler state.
        [[nodiscard]] Result<void> terminate(ProcessId pid, Tick currentTick) override;

        /// @brief Snapshot of ready queue process IDs.
        [[nodiscard]] std::vector<ProcessId> getQueueSnapshot() const override;

        /// @brief Snapshot of blocked queue process IDs.
        [[nodiscard]] std::vector<ProcessId> getBlockedSnapshot() const override;

        /// @brief Configures per-core lane count for ready queues.
        [[nodiscard]] Result<void> configureLanes(std::size_t laneCount) override;

        /// @brief Configured scheduler lane count.
        [[nodiscard]] std::size_t laneCount() const noexcept override;

        /// @brief Enqueues process into a specific lane.
        [[nodiscard]] Result<void> enqueueToLane(PCB &pcb, std::size_t laneIndex, Tick currentTick) override;

        /// @brief Selects next process for a specific lane.
        [[nodiscard]] Result<ProcessId> selectNextForLane(std::size_t laneIndex, const IClock &clock) override;

        /// @brief Steals work for thief lane and selects next process there.
        [[nodiscard]] Result<ProcessId> stealNextForLane(std::size_t thiefLane, const IClock &clock) override;

        /// @brief Snapshot of ready queues per lane.
        [[nodiscard]] std::vector<std::vector<ProcessId>> getPerLaneQueueSnapshot() const override;

        /// @brief Currently running process IDs across scheduler lanes.
        [[nodiscard]] std::vector<ProcessId> runningProcesses() const override;

        /// @brief Replaces the active scheduling policy.
        [[nodiscard]] Result<void> setPolicy(std::unique_ptr<ISchedulingPolicy> policy) override;

        /// @brief Returns active policy name.
        [[nodiscard]] std::string_view policyName() const noexcept;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
