/// @file mutex.h
/// @brief Mutex synchronization primitive.

#pragma once

#include <memory>
#include <optional>

#include "contur/sync/i_sync_primitive.h"

namespace contur {

    /// @brief Reentrant mutex with ownership tracking.
    class Mutex final : public ISyncPrimitive
    {
        public:
        /// @brief Constructs an unlocked mutex.
        Mutex();
        ~Mutex() override;

        Mutex(const Mutex &) = delete;
        Mutex &operator=(const Mutex &) = delete;
        Mutex(Mutex &&) noexcept;
        Mutex &operator=(Mutex &&) noexcept;

        /// @brief Acquires the mutex for process pid.
        /// @return Ok when acquired; ResourceBusy when owned by another process.
        [[nodiscard]] Result<void> acquire(ProcessId pid) override;

        /// @brief Releases the mutex held by process pid.
        /// @return Ok on success; PermissionDenied/InvalidState on misuse.
        [[nodiscard]] Result<void> release(ProcessId pid) override;

        /// @brief Attempts immediate acquire without enqueueing waiters.
        [[nodiscard]] Result<void> tryAcquire(ProcessId pid) override;

        /// @brief Primitive name for diagnostics.
        [[nodiscard]] std::string_view name() const noexcept override;

        /// @brief True when the mutex is owned by some process.
        [[nodiscard]] bool isLocked() const noexcept;

        /// @brief Current owner process ID, if locked.
        [[nodiscard]] std::optional<ProcessId> owner() const noexcept;

        /// @brief Reentrancy depth for owner.
        [[nodiscard]] std::size_t recursionDepth() const noexcept;

        /// @brief Number of waiting processes.
        [[nodiscard]] std::size_t waitingCount() const noexcept;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
