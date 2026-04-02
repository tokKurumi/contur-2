/// @file trace_level.h
/// @brief Trace severity levels used by the tracing subsystem.

#pragma once

#include <cstdint>
#include <string_view>

namespace contur {

    /// @brief Trace event severity.
    enum class TraceLevel : std::uint8_t
    {
        Debug = 0,
        Info = 1,
        Warn = 2,
        Error = 3,
    };

    /// @brief Converts TraceLevel to a human-readable string.
    [[nodiscard]] constexpr std::string_view traceLevelToString(TraceLevel level) noexcept
    {
        switch (level)
        {
        case TraceLevel::Debug:
            return "debug";
        case TraceLevel::Info:
            return "info";
        case TraceLevel::Warn:
            return "warn";
        case TraceLevel::Error:
            return "error";
        }
        return "unknown";
    }

} // namespace contur
