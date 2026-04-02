/// @file threading_config.h
/// @brief Runtime-owned host threading configuration for dispatch runtimes.

#pragma once

#include <cstddef>

namespace contur {

    /// @brief Configuration for host-thread dispatch runtime behavior.
    struct HostThreadingConfig
    {
        /// @brief Number of host worker threads (must be >= 1).
        std::size_t hostThreadCount = 1;

        /// @brief Enables deterministic scheduling checkpoints for multithreaded runs.
        bool deterministicMode = true;

        /// @brief Enables work stealing between worker lanes when multithreading is active.
        bool workStealingEnabled = false;

        /// @brief Returns true when this config satisfies minimum runtime requirements.
        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return hostThreadCount >= 1U;
        }

        /// @brief Returns true when the runtime is configured for a single host thread.
        [[nodiscard]] constexpr bool isSingleThreaded() const noexcept
        {
            return hostThreadCount == 1U;
        }

        /// @brief Returns a normalized copy with safe baseline invariants.
        [[nodiscard]] constexpr HostThreadingConfig normalized() const noexcept
        {
            HostThreadingConfig normalizedConfig = *this;

            if (normalizedConfig.hostThreadCount == 0U)
            {
                normalizedConfig.hostThreadCount = 1U;
            }

            if (normalizedConfig.isSingleThreaded())
            {
                normalizedConfig.workStealingEnabled = false;
            }

            return normalizedConfig;
        }

        /// @brief Normalizes this instance in place.
        constexpr void normalize() noexcept
        {
            *this = normalized();
        }
    };

} // namespace contur