/// @file critical_section.h
/// @brief CriticalSection wrapper composed over an ISyncPrimitive.

#pragma once

#include <memory>

#include "contur/sync/i_sync_primitive.h"

namespace contur {

    /// @brief Critical-section adapter over a synchronization primitive.
    class CriticalSection final : public ISyncPrimitive
    {
        public:
        /// @brief Constructs a critical section using provided primitive.
        /// @details If primitive is null, an internal Mutex is used.
        explicit CriticalSection(std::unique_ptr<ISyncPrimitive> primitive = nullptr);

        /// @brief Destroys critical-section adapter.
        ~CriticalSection() override;

        /// @brief Copy construction is disabled.
        CriticalSection(const CriticalSection &) = delete;

        /// @brief Copy assignment is disabled.
        CriticalSection &operator=(const CriticalSection &) = delete;
        /// @brief Move-constructs critical section state.
        CriticalSection(CriticalSection &&) noexcept;

        /// @brief Move-assigns critical section state.
        CriticalSection &operator=(CriticalSection &&) noexcept;

        /// @brief Acquires underlying primitive.
        [[nodiscard]] Result<void> acquire(ProcessId pid) override;

        /// @brief Releases underlying primitive.
        [[nodiscard]] Result<void> release(ProcessId pid) override;

        /// @brief Non-blocking acquire on underlying primitive.
        [[nodiscard]] Result<void> tryAcquire(ProcessId pid) override;

        /// @brief Primitive name for diagnostics.
        [[nodiscard]] std::string_view name() const noexcept override;

        /// @brief Layer classification forwarded from underlying primitive.
        [[nodiscard]] SyncLayer layer() const noexcept override;

        /// @brief Alias for acquire().
        [[nodiscard]] Result<void> enter(ProcessId pid);

        /// @brief Alias for release().
        [[nodiscard]] Result<void> leave(ProcessId pid);

        /// @brief Alias for tryAcquire().
        [[nodiscard]] Result<void> tryEnter(ProcessId pid);

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
