/// @file priority.h
/// @brief Process priority levels and the Priority struct.
///
/// Priority is a first-class concept in Contur 2. Each process has a base
/// priority (set at creation), an effective priority (dynamically adjusted by
/// the scheduler), and a nice value for fine-grained tuning.

#pragma once

#include <cstdint>
#include <string_view>

namespace contur {

    /// @brief Discrete priority levels, from highest (Realtime) to lowest (Idle).
    ///
    /// Numerically lower values represent higher priority. This ordering allows
    /// natural comparison: `Realtime < High < Normal < Low < Idle`.
    enum class PriorityLevel : std::int8_t
    {
        Realtime = 0,    ///< Highest — time-critical, preempts everything.
        High = 1,        ///< Above-normal urgency.
        AboveNormal = 2, ///< Slightly above default.
        Normal = 3,      ///< Default priority for user processes.
        BelowNormal = 4, ///< Slightly below default.
        Low = 5,         ///< Background tasks.
        Idle = 6,        ///< Lowest — runs only when nothing else is ready.
    };

    /// @brief Minimum nice value (highest boost).
    constexpr std::int32_t NICE_MIN = -20;

    /// @brief Maximum nice value (lowest boost).
    constexpr std::int32_t NICE_MAX = 19;

    /// @brief Default nice value (no adjustment).
    constexpr std::int32_t NICE_DEFAULT = 0;

    /// @brief Composite priority descriptor for a process.
    ///
    /// - `base`      — assigned at creation, does not change during normal execution.
    /// - `effective`  — adjusted dynamically by the scheduler (e.g., starvation prevention,
    ///                  I/O boost, priority inheritance).
    /// - `nice`       — Unix-style fine-grained adjustment (range: NICE_MIN..NICE_MAX).
    struct Priority
    {
        PriorityLevel base = PriorityLevel::Normal;      ///< Static priority assigned at creation.
        PriorityLevel effective = PriorityLevel::Normal; ///< Dynamic priority used by scheduler.
        std::int32_t nice = NICE_DEFAULT;                ///< Fine-grained adjustment (-20..+19).

        /// @brief Default-constructs a Normal-priority descriptor.
        constexpr Priority() noexcept = default;

        /// @brief Constructs a priority with the given base level.
        ///        Effective is initialized to match base; nice defaults to 0.
        constexpr explicit Priority(PriorityLevel level) noexcept
            : base(level)
            , effective(level)
            , nice(NICE_DEFAULT)
        {}

        /// @brief Constructs a priority with base, effective, and nice values.
        constexpr Priority(PriorityLevel base, PriorityLevel effective, std::int32_t nice) noexcept
            : base(base)
            , effective(effective)
            , nice(clampNice(nice))
        {}

        /// @brief Compares two Priority descriptors for equality.
        [[nodiscard]] constexpr bool operator==(const Priority &other) const noexcept
        {
            return base == other.base && effective == other.effective && nice == other.nice;
        }

        /// @brief Compares two Priority descriptors for inequality.
        [[nodiscard]] constexpr bool operator!=(const Priority &other) const noexcept
        {
            return !(*this == other);
        }

        /// @brief Returns true if this priority is higher (numerically lower) than other,
        ///        based on effective level. Tie-broken by nice value (lower nice = higher prio).
        [[nodiscard]] constexpr bool isHigherThan(const Priority &other) const noexcept
        {
            if (effective != other.effective)
            {
                return static_cast<std::int8_t>(effective) < static_cast<std::int8_t>(other.effective);
            }
            return nice < other.nice;
        }

        /// @brief Clamps a nice value to the valid range [NICE_MIN, NICE_MAX].
        [[nodiscard]] static constexpr std::int32_t clampNice(std::int32_t value) noexcept
        {
            if (value < NICE_MIN)
            {
                return NICE_MIN;
            }
            if (value > NICE_MAX)
            {
                return NICE_MAX;
            }
            return value;
        }
    };

    /// @brief Returns a human-readable name for the given priority level.
    [[nodiscard]] constexpr std::string_view priorityLevelName(PriorityLevel level) noexcept
    {
        switch (level)
        {
        case PriorityLevel::Realtime:
            return "Realtime";
        case PriorityLevel::High:
            return "High";
        case PriorityLevel::AboveNormal:
            return "AboveNormal";
        case PriorityLevel::Normal:
            return "Normal";
        case PriorityLevel::BelowNormal:
            return "BelowNormal";
        case PriorityLevel::Low:
            return "Low";
        case PriorityLevel::Idle:
            return "Idle";
        }
        return "Unknown";
    }

} // namespace contur
