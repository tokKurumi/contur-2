/// @file semaphore.h
/// @brief Counting semaphore synchronization primitive.

#pragma once

#include <memory>

#include "contur/process/priority.h"
#include "contur/sync/i_sync_primitive.h"

namespace contur {

    /// @brief Counting semaphore synchronization primitive.
    class Semaphore final : public ISyncPrimitive
    {
        public:
        /// @brief Constructs semaphore with initial and maximum count.
        explicit Semaphore(std::size_t initialCount = 1, std::size_t maxCount = 1);

        /// @brief Destroys semaphore.
        ~Semaphore() override;

        /// @brief Copy construction is disabled.
        Semaphore(const Semaphore &) = delete;

        /// @brief Copy assignment is disabled.
        Semaphore &operator=(const Semaphore &) = delete;
        /// @brief Move-constructs semaphore state.
        Semaphore(Semaphore &&) noexcept;

        /// @brief Move-assigns semaphore state.
        Semaphore &operator=(Semaphore &&) noexcept;

        /// @brief Acquires one semaphore unit.
        [[nodiscard]] Result<void> acquire(ProcessId pid) override;

        /// @brief Releases one semaphore unit.
        [[nodiscard]] Result<void> release(ProcessId pid) override;

        /// @brief Non-blocking acquire attempt.
        [[nodiscard]] Result<void> tryAcquire(ProcessId pid) override;

        /// @brief Primitive name for diagnostics.
        [[nodiscard]] std::string_view name() const noexcept override;

        /// @brief Layer classification for synchronization model split.
        [[nodiscard]] SyncLayer layer() const noexcept override;

        /// @brief Registers process base priority used for boost rules.
        [[nodiscard]] Result<void> registerProcessPriority(ProcessId pid, PriorityLevel basePriority);

        /// @brief Effective priority after inheritance boosts.
        [[nodiscard]] PriorityLevel effectivePriority(ProcessId pid) const noexcept;

        /// @brief Registered base priority.
        [[nodiscard]] PriorityLevel basePriority(ProcessId pid) const noexcept;

        /// @brief Current available count.
        [[nodiscard]] std::size_t count() const noexcept;

        /// @brief Configured maximum count.
        [[nodiscard]] std::size_t maxCount() const noexcept;

        /// @brief Number of waiting processes.
        [[nodiscard]] std::size_t waitingCount() const noexcept;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
