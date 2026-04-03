/// @file tui_commands.h
/// @brief Command contracts and validation helpers for TUI controller behavior.

#pragma once

#include <cstddef>
#include <cstdint>

#include "contur/core/error.h"

namespace contur {

    /// @brief Supported command kinds for TUI playback/navigation.
    enum class TuiCommandKind : std::uint8_t
    {
        Tick,
        AutoPlayStart,
        AutoPlayStop,
        Pause,
        SeekBackward,
        SeekForward,
    };

    /// @brief Autoplay configuration used by controller/runtime scheduling logic.
    struct TuiPlaybackConfig
    {
        /// @brief Autoplay interval in milliseconds.
        std::uint32_t intervalMs = 100;

        /// @brief Number of ticks per autoplay step.
        std::size_t step = 1;
    };

    /// @brief Generic command payload accepted by TUI controller.
    struct TuiCommand
    {
        /// @brief Command discriminator.
        TuiCommandKind kind = TuiCommandKind::Tick;

        /// @brief Step size used by tick/seek/autoplay commands.
        std::size_t step = 1;

        /// @brief Interval in milliseconds used by autoplay start.
        std::uint32_t intervalMs = 100;
    };

    /// @brief Returns true when command kind requires non-zero step value.
    /// @param kind Command kind.
    /// @return True for step-based commands.
    [[nodiscard]] constexpr bool commandRequiresStep(TuiCommandKind kind) noexcept
    {
        return kind == TuiCommandKind::Tick || kind == TuiCommandKind::AutoPlayStart ||
               kind == TuiCommandKind::SeekBackward || kind == TuiCommandKind::SeekForward;
    }

    /// @brief Returns true when command kind requires non-zero interval value.
    /// @param kind Command kind.
    /// @return True only for autoplay-start commands.
    [[nodiscard]] constexpr bool commandRequiresInterval(TuiCommandKind kind) noexcept
    {
        return kind == TuiCommandKind::AutoPlayStart;
    }

    /// @brief Validates autoplay configuration.
    /// @param config Playback configuration to validate.
    /// @return Ok for valid config or InvalidArgument on invalid fields.
    [[nodiscard]] inline Result<void> validatePlaybackConfig(const TuiPlaybackConfig &config)
    {
        if (config.step == 0 || config.intervalMs == 0)
        {
            return Result<void>::error(ErrorCode::InvalidArgument);
        }
        return Result<void>::ok();
    }

    /// @brief Validates a TUI command payload.
    /// @param command Command payload.
    /// @return Ok for valid command fields or InvalidArgument.
    [[nodiscard]] inline Result<void> validateCommand(const TuiCommand &command)
    {
        if (commandRequiresStep(command.kind) && command.step == 0)
        {
            return Result<void>::error(ErrorCode::InvalidArgument);
        }
        if (commandRequiresInterval(command.kind) && command.intervalMs == 0)
        {
            return Result<void>::error(ErrorCode::InvalidArgument);
        }
        return Result<void>::ok();
    }

    /// @brief Builds playback configuration from command payload values.
    /// @param command Command payload.
    /// @return Playback configuration using command interval/step values.
    [[nodiscard]] constexpr TuiPlaybackConfig playbackConfigFromCommand(const TuiCommand &command) noexcept
    {
        return TuiPlaybackConfig{command.intervalMs, command.step};
    }

} // namespace contur
