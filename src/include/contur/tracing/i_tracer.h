/// @file i_tracer.h
/// @brief Tracer interface for hierarchical event tracing.

#pragma once

#include <cstdint>
#include <string_view>

#include "contur/tracing/trace_event.h"

namespace contur {

    class IClock;

    /// @brief Tracer interface used by kernel subsystems.
    class ITracer
    {
        public:
        /// @brief Virtual destructor for polymorphic cleanup.
        virtual ~ITracer() = default;

        /// @brief Emits a pre-constructed trace event.
        /// @param event Trace event to emit.
        virtual void trace(const TraceEvent &event) = 0;

        /// @brief Enters a nested tracing scope.
        /// @param subsystem Subsystem name.
        /// @param operation Operation name.
        virtual void pushScope(std::string_view subsystem, std::string_view operation) = 0;

        /// @brief Leaves the current tracing scope.
        virtual void popScope() = 0;

        /// @brief Returns current tracing depth for this tracer.
        /// @return Current scope depth.
        [[nodiscard]] virtual std::uint32_t currentDepth() const noexcept = 0;

        /// @brief Sets minimum event level that will be emitted.
        /// @param level Minimum accepted level.
        virtual void setMinLevel(TraceLevel level) noexcept = 0;

        /// @brief Returns minimum event level that will be emitted.
        [[nodiscard]] virtual TraceLevel minLevel() const noexcept = 0;

        /// @brief Returns tracer clock reference.
        /// @return Clock used for timestamping events.
        [[nodiscard]] virtual const IClock &clock() const noexcept = 0;
    };

} // namespace contur
