/// @file trace_scope.h
/// @brief RAII tracing scope helper and trace macros.

#pragma once

#include <string_view>

#include "contur/tracing/i_tracer.h"

namespace contur {

    /// @brief RAII helper that pushes/pops tracer scope automatically.
    class TraceScope final
    {
        public:
        /// @brief Constructs a nested tracing scope.
        /// @param tracer Tracer instance.
        /// @param subsystem Subsystem name.
        /// @param operation Operation name.
        TraceScope(ITracer &tracer, std::string_view subsystem, std::string_view operation)
            : tracer_(tracer)
            , active_(true)
        {
            tracer_.pushScope(subsystem, operation);
        }

        /// @brief Destroys scope and pops nesting level.
        ~TraceScope()
        {
            if (active_)
            {
                tracer_.popScope();
            }
        }

        /// @brief Copy construction is disabled.
        TraceScope(const TraceScope &) = delete;

        /// @brief Copy assignment is disabled.
        TraceScope &operator=(const TraceScope &) = delete;
        /// @brief Move construction is disabled.
        TraceScope(TraceScope &&) = delete;

        /// @brief Move assignment is disabled.
        TraceScope &operator=(TraceScope &&) = delete;

        private:
        ITracer &tracer_;
        bool active_;
    };

} // namespace contur

#ifdef CONTUR_TRACE_ENABLED
#define CONTUR_TRACE_SCOPE(tracer, subsystem, operation) \
    ::contur::TraceScope contur_trace_scope_##__LINE__((tracer), (subsystem), (operation))
#define CONTUR_TRACE_L(tracer, level, subsystem, operation, details)                                      \
    (tracer).trace(                                                                                       \
        ::contur::makeTraceEvent(                                                                         \
            (tracer).clock().now(), (subsystem), (operation), (details), (tracer).currentDepth(), (level) \
        )                                                                                                 \
    )
#define CONTUR_TRACE(tracer, subsystem, operation, details) \
    CONTUR_TRACE_L((tracer), ::contur::TraceLevel::Info, (subsystem), (operation), (details))
#else
#define CONTUR_TRACE_SCOPE(tracer, subsystem, operation) ((void)0)
#define CONTUR_TRACE_L(tracer, level, subsystem, operation, details) ((void)0)
#define CONTUR_TRACE(tracer, subsystem, operation, details) ((void)0)
#endif
