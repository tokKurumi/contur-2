/// @file semaphore.h
/// @brief Counting semaphore synchronization primitive.

#pragma once

#include <memory>

#include "contur/sync/i_sync_primitive.h"

namespace contur {

    /// @brief Counting semaphore synchronization primitive.
    class Semaphore final : public ISyncPrimitive
    {
        public:
        /// @brief Constructs semaphore with initial and maximum count.
        explicit Semaphore(std::size_t initialCount = 1, std::size_t maxCount = 1);
        ~Semaphore() override;

        Semaphore(const Semaphore &) = delete;
        Semaphore &operator=(const Semaphore &) = delete;
        Semaphore(Semaphore &&) noexcept;
        Semaphore &operator=(Semaphore &&) noexcept;

        /// @brief Acquires one semaphore unit.
        [[nodiscard]] Result<void> acquire(ProcessId pid) override;

        /// @brief Releases one semaphore unit.
        [[nodiscard]] Result<void> release(ProcessId pid) override;

        /// @brief Non-blocking acquire attempt.
        [[nodiscard]] Result<void> tryAcquire(ProcessId pid) override;

        /// @brief Primitive name for diagnostics.
        [[nodiscard]] std::string_view name() const noexcept override;

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
