/// @file i_tui_controller.h
/// @brief TUI controller contracts and default controller implementation.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

#include "contur/core/error.h"

#include "contur/tui/i_kernel_read_model.h"
#include "contur/tui/tui_commands.h"
#include "contur/tui/tui_models.h"

namespace contur {

    /// @brief Controller state for playback lifecycle.
    enum class TuiControllerState : std::uint8_t
    {
        Idle,
        Playing,
        Paused,
    };

    /// @brief TUI controller interface for command/state orchestration.
    class ITuiController
    {
        public:
        /// @brief Virtual destructor for interface-safe polymorphic cleanup.
        virtual ~ITuiController() = default;

        /// @brief Dispatches one controller command.
        /// @param command Command payload.
        /// @return Ok on success or an error for invalid command/state.
        [[nodiscard]] virtual Result<void> dispatch(const TuiCommand &command) = 0;

        /// @brief Advances autoplay timer and executes due tick steps.
        /// @param elapsedMs Elapsed wall-clock milliseconds since previous call.
        /// @return Ok on success or propagated tick/read-model error.
        [[nodiscard]] virtual Result<void> advanceAutoplay(std::uint32_t elapsedMs) = 0;

        /// @brief Returns current UI snapshot at history cursor.
        [[nodiscard]] virtual const TuiSnapshot &current() const noexcept = 0;

        /// @brief Returns current playback state.
        [[nodiscard]] virtual TuiControllerState state() const noexcept = 0;

        /// @brief Returns retained history entry count.
        [[nodiscard]] virtual std::size_t historySize() const noexcept = 0;

        /// @brief Returns current history cursor index.
        [[nodiscard]] virtual std::size_t historyCursor() const noexcept = 0;
    };

    /// @brief Default MVC controller implementation.
    class TuiController final : public ITuiController
    {
        public:
        /// @brief Signature used by controller to advance kernel time.
        using TickFn = std::function<Result<void>(std::size_t step)>;

        /// @brief Constructs controller.
        /// @param readModel Read-model adapter for snapshot capture.
        /// @param tickFn Callback that advances kernel by N ticks.
        /// @param historyCapacity Max retained history entries (0 normalizes to 1).
        TuiController(const IKernelReadModel &readModel, TickFn tickFn, std::size_t historyCapacity = 256);

        /// @brief Destroys controller.
        ~TuiController() override;

        /// @brief Copy construction is disabled.
        TuiController(const TuiController &) = delete;

        /// @brief Copy assignment is disabled.
        TuiController &operator=(const TuiController &) = delete;

        /// @brief Move-constructs controller state.
        TuiController(TuiController &&) noexcept;

        /// @brief Move-assigns controller state.
        TuiController &operator=(TuiController &&) noexcept;

        /// @copydoc ITuiController::dispatch
        [[nodiscard]] Result<void> dispatch(const TuiCommand &command) override;

        /// @copydoc ITuiController::advanceAutoplay
        [[nodiscard]] Result<void> advanceAutoplay(std::uint32_t elapsedMs) override;

        /// @copydoc ITuiController::current
        [[nodiscard]] const TuiSnapshot &current() const noexcept override;

        /// @copydoc ITuiController::state
        [[nodiscard]] TuiControllerState state() const noexcept override;

        /// @copydoc ITuiController::historySize
        [[nodiscard]] std::size_t historySize() const noexcept override;

        /// @copydoc ITuiController::historyCursor
        [[nodiscard]] std::size_t historyCursor() const noexcept override;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
