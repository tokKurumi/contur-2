/// @file i_sync_primitive.h
/// @brief ISyncPrimitive interface for synchronization objects.

#pragma once

#include <string_view>

#include "contur/core/error.h"
#include "contur/core/types.h"

namespace contur {

    /// @brief Common interface for synchronization primitives.
    class ISyncPrimitive
    {
        public:
        virtual ~ISyncPrimitive() = default;

        /// @brief Attempts to acquire the primitive for a process.
        /// @param pid Caller process ID.
        /// @return Ok on success or an error code.
        [[nodiscard]] virtual Result<void> acquire(ProcessId pid) = 0;

        /// @brief Releases the primitive for a process.
        /// @param pid Releasing process ID.
        /// @return Ok on success or an error code.
        [[nodiscard]] virtual Result<void> release(ProcessId pid) = 0;

        /// @brief Attempts non-blocking acquire.
        /// @param pid Caller process ID.
        /// @return Ok when acquired, or ResourceBusy/error.
        [[nodiscard]] virtual Result<void> tryAcquire(ProcessId pid) = 0;

        /// @brief Human-readable primitive name.
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    };

} // namespace contur
