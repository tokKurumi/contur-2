/// @file i_process.h
/// @brief IProcess — read-only interface for external consumers of process info.
///
/// External subsystems (TUI views, statistics, tracing) use this interface to
/// inspect process metadata without being able to modify it. The Dispatcher and
/// Scheduler work with the concrete PCB/ProcessImage directly.

#pragma once

#include <string_view>

#include "contur/core/types.h"

#include "contur/process/priority.h"
#include "contur/process/state.h"

namespace contur {

    struct ProcessTiming;

    /// @brief Read-only interface for inspecting process metadata.
    ///
    /// Provides const access to process identity, state, priority, and timing.
    /// Implemented by ProcessImage (or any adapter wrapping a PCB).
    class IProcess
    {
        public:
        virtual ~IProcess() = default;

        /// @brief Returns the unique process ID.
        /// @return Stable process identifier assigned at creation time.
        [[nodiscard]] virtual ProcessId id() const noexcept = 0;

        /// @brief Returns the human-readable process name.
        /// @return Non-owning view of the process name.
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;

        /// @brief Returns the current process state.
        /// @return Current lifecycle state of the process.
        [[nodiscard]] virtual ProcessState state() const noexcept = 0;

        /// @brief Returns the process priority descriptor.
        /// @return Reference to the process priority metadata.
        [[nodiscard]] virtual const Priority &priority() const noexcept = 0;

        /// @brief Returns the process timing statistics.
        /// @return Reference to process timing counters and timestamps.
        [[nodiscard]] virtual const ProcessTiming &timing() const noexcept = 0;

        protected:
        IProcess() = default;

        // Non-copyable, non-movable (interface)
        IProcess(const IProcess &) = default;
        IProcess &operator=(const IProcess &) = default;
        IProcess(IProcess &&) = default;
        IProcess &operator=(IProcess &&) = default;
    };

} // namespace contur
