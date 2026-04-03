/// @file trace_event.h
/// @brief Structured trace event record shared by tracing components.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "contur/core/types.h"

#include "contur/tracing/trace_level.h"

namespace contur {

    /// @brief Structured trace event record.
    ///
    /// Used by both the tracing subsystem (`Tracer` + sinks) and the
    /// dispatcher runtime metadata emitted by `DispatcherPool`.
    struct TraceEvent
    {
        /// @brief Simulation timestamp when the event was emitted.
        Tick timestamp = 0;

        /// @brief Subsystem that emitted the event.
        std::string subsystem;

        /// @brief Operation name within the subsystem.
        std::string operation;

        /// @brief Optional human-readable details payload.
        std::string details;

        /// @brief Event severity level.
        TraceLevel level = TraceLevel::Info;

        /// @brief Current tracer nesting depth.
        std::uint32_t depth = 0;

        /// @brief Worker index used by concurrent dispatcher runtime.
        std::size_t workerId = 0;

        /// @brief Monotonic sequence value used for canonical ordering.
        std::uint64_t sequence = 0;

        /// @brief Runtime epoch identifier used for deterministic replay checks.
        std::uint64_t epoch = 0;
    };

    /// @brief Convenience helper for constructing a trace event.
    /// @param timestamp Event timestamp.
    /// @param subsystem Subsystem that emits the event.
    /// @param operation Operation name.
    /// @param details Optional details string.
    /// @param depth Current nesting depth.
    /// @param level Event severity.
    /// @return Fully initialized TraceEvent value.
    [[nodiscard]] inline TraceEvent makeTraceEvent(
        Tick timestamp,
        std::string_view subsystem,
        std::string_view operation,
        std::string_view details,
        std::uint32_t depth,
        TraceLevel level = TraceLevel::Info
    )
    {
        TraceEvent event;
        event.timestamp = timestamp;
        event.subsystem = std::string(subsystem);
        event.operation = std::string(operation);
        event.details = std::string(details);
        event.level = level;
        event.depth = depth;
        return event;
    }

} // namespace contur
