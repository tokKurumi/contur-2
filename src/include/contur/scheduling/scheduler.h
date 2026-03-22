/// @file scheduler.h
/// @brief Scheduler implementation hosting a pluggable policy.

#pragma once

#include <memory>

#include "contur/scheduling/i_scheduler.h"

namespace contur {

    /// @brief Scheduler host that manages process queues and policy decisions.
    ///
    /// Scheduler owns queue state and delegates ordering/preemption logic
    /// to the active ISchedulingPolicy implementation.
    class Scheduler final : public IScheduler
    {
        public:
        /// @brief Constructs scheduler with initial policy.
        /// @param policy Non-null scheduling policy implementation.
        explicit Scheduler(std::unique_ptr<ISchedulingPolicy> policy);
        ~Scheduler() override;

        Scheduler(const Scheduler &) = delete;
        Scheduler &operator=(const Scheduler &) = delete;
        Scheduler(Scheduler &&) noexcept;
        Scheduler &operator=(Scheduler &&) noexcept;

        /// @brief Enqueues process into ready queue.
        [[nodiscard]] Result<void> enqueue(PCB &pcb, Tick currentTick) override;

        /// @brief Removes process from scheduler tracking.
        [[nodiscard]] Result<void> dequeue(ProcessId pid) override;

        /// @brief Selects next process according to active policy.
        [[nodiscard]] Result<ProcessId> selectNext(const IClock &clock) override;

        /// @brief Moves running process to blocked state.
        [[nodiscard]] Result<void> blockRunning(Tick currentTick) override;

        /// @brief Moves blocked process back to ready queue.
        [[nodiscard]] Result<void> unblock(ProcessId pid, Tick currentTick) override;

        /// @brief Terminates process and removes it from scheduler state.
        [[nodiscard]] Result<void> terminate(ProcessId pid, Tick currentTick) override;

        /// @brief Snapshot of ready queue process IDs.
        [[nodiscard]] std::vector<ProcessId> getQueueSnapshot() const override;

        /// @brief Snapshot of blocked queue process IDs.
        [[nodiscard]] std::vector<ProcessId> getBlockedSnapshot() const override;

        /// @brief Currently running process ID or INVALID_PID.
        [[nodiscard]] ProcessId runningProcess() const noexcept override;

        /// @brief Replaces the active scheduling policy.
        [[nodiscard]] Result<void> setPolicy(std::unique_ptr<ISchedulingPolicy> policy) override;

        /// @brief Returns active policy name.
        [[nodiscard]] std::string_view policyName() const noexcept;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
