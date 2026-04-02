/// @file trace_event.h
/// @brief Lightweight concurrent trace event metadata bridge.

#pragma once

#include <cstdint>
#include <string>

#include "contur/core/types.h"

namespace contur {

    /// @brief Structured trace event used by concurrent runtime metadata.
    struct TraceEvent
    {
        Tick timestamp = 0;
        std::string subsystem;
        std::string operation;
        std::string details;
        std::size_t depth = 0;

        // Concurrent-runtime metadata for replay diagnostics.
        std::size_t workerId = 0;
        std::uint64_t sequence = 0;
        std::uint64_t epoch = 0;
    };

} // namespace contur
