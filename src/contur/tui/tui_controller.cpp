/// @file tui_controller.cpp
/// @brief TuiController implementation.

#include "contur/tui/i_tui_controller.h"

#include <utility>

#include "contur/tui/history_buffer.h"

namespace contur {

    struct TuiController::Impl
    {
        const IKernelReadModel &readModel;
        TickFn tick;
        HistoryBuffer history;
        TuiSnapshot current;
        TuiPlaybackConfig playback;
        TuiControllerState state = TuiControllerState::Idle;
        std::uint32_t accumulatedMs = 0;
        std::uint64_t nextSequence = 0;

        Impl(const IKernelReadModel &readModelRef, TickFn tickFn, std::size_t historyCapacity)
            : readModel(readModelRef)
            , tick(std::move(tickFn))
            , history(historyCapacity)
        {}

        Result<void> syncCurrentFromCursor()
        {
            auto entry = history.current();
            if (!entry.has_value())
            {
                return Result<void>::error(ErrorCode::NotFound);
            }

            current = entry->get().snapshot;
            return Result<void>::ok();
        }

        Result<void> captureSnapshot()
        {
            auto captured = readModel.captureSnapshot();
            if (captured.isError())
            {
                return Result<void>::error(captured.errorCode());
            }

            TuiSnapshot snapshot = std::move(captured).value();
            if (snapshot.sequence == 0)
            {
                snapshot.sequence = ++nextSequence;
            }
            else if (snapshot.sequence > nextSequence)
            {
                nextSequence = snapshot.sequence;
            }

            TuiHistoryEntry entry;
            entry.sequence = static_cast<std::size_t>(snapshot.sequence);
            entry.snapshot = std::move(snapshot);

            auto appended = history.append(std::move(entry));
            if (appended.isError())
            {
                return appended;
            }

            return syncCurrentFromCursor();
        }

        Result<void> tickAndCapture(std::size_t step)
        {
            auto tickResult = tick(step);
            if (tickResult.isError())
            {
                return tickResult;
            }
            return captureSnapshot();
        }
    };

    TuiController::TuiController(const IKernelReadModel &readModel, TickFn tickFn, std::size_t historyCapacity)
        : impl_(std::make_unique<Impl>(readModel, std::move(tickFn), historyCapacity))
    {
        if (impl_)
        {
            (void)impl_->captureSnapshot();
        }
    }

    TuiController::~TuiController() = default;
    TuiController::TuiController(TuiController &&) noexcept = default;
    TuiController &TuiController::operator=(TuiController &&) noexcept = default;

    Result<void> TuiController::dispatch(const TuiCommand &command)
    {
        if (!impl_)
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        auto valid = validateCommand(command);
        if (valid.isError())
        {
            return valid;
        }

        switch (command.kind)
        {
        case TuiCommandKind::Tick:
            return impl_->tickAndCapture(command.step);

        case TuiCommandKind::AutoPlayStart: {
            TuiPlaybackConfig config = playbackConfigFromCommand(command);
            auto playbackValid = validatePlaybackConfig(config);
            if (playbackValid.isError())
            {
                return playbackValid;
            }
            impl_->playback = config;
            impl_->accumulatedMs = 0;
            impl_->state = TuiControllerState::Playing;
            return Result<void>::ok();
        }

        case TuiCommandKind::AutoPlayStop:
            impl_->accumulatedMs = 0;
            impl_->state = TuiControllerState::Idle;
            return Result<void>::ok();

        case TuiCommandKind::Pause:
            if (impl_->state == TuiControllerState::Playing)
            {
                impl_->state = TuiControllerState::Paused;
            }
            return Result<void>::ok();

        case TuiCommandKind::SeekBackward: {
            auto seek = impl_->history.seekBackward(command.step);
            if (seek.isError())
            {
                return seek;
            }
            return impl_->syncCurrentFromCursor();
        }

        case TuiCommandKind::SeekForward: {
            auto seek = impl_->history.seekForward(command.step);
            if (seek.isError())
            {
                return seek;
            }
            return impl_->syncCurrentFromCursor();
        }
        }

        return Result<void>::error(ErrorCode::InvalidArgument);
    }

    Result<void> TuiController::advanceAutoplay(std::uint32_t elapsedMs)
    {
        if (!impl_)
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        if (impl_->state != TuiControllerState::Playing)
        {
            return Result<void>::ok();
        }

        if (impl_->playback.intervalMs == 0 || impl_->playback.step == 0)
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        impl_->accumulatedMs += elapsedMs;
        while (impl_->accumulatedMs >= impl_->playback.intervalMs)
        {
            impl_->accumulatedMs -= impl_->playback.intervalMs;

            auto ticked = impl_->tickAndCapture(impl_->playback.step);
            if (ticked.isError())
            {
                impl_->state = TuiControllerState::Paused;
                return ticked;
            }
        }

        return Result<void>::ok();
    }

    const TuiSnapshot &TuiController::current() const noexcept
    {
        return impl_->current;
    }

    TuiControllerState TuiController::state() const noexcept
    {
        if (!impl_)
        {
            return TuiControllerState::Idle;
        }
        return impl_->state;
    }

    std::size_t TuiController::historySize() const noexcept
    {
        if (!impl_)
        {
            return 0;
        }
        return impl_->history.size();
    }

    std::size_t TuiController::historyCursor() const noexcept
    {
        if (!impl_)
        {
            return 0;
        }
        return impl_->history.cursor();
    }

} // namespace contur
