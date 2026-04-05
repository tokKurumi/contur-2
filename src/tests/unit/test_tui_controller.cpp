/// @file test_tui_controller.cpp
/// @brief Unit tests for TUI controller state machine semantics.

#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "contur/core/error.h"

#include "contur/tui/i_tui_controller.h"

using namespace contur;

namespace {

    class FakeKernelReadModel final : public IKernelReadModel
    {
        public:
        explicit FakeKernelReadModel(const Tick &tickRef)
            : tickRef_(tickRef)
        {}

        [[nodiscard]] Result<TuiSnapshot> captureSnapshot() const override
        {
            ++captureCalls_;
            if (captureError_ != ErrorCode::Ok)
            {
                return Result<TuiSnapshot>::error(captureError_);
            }

            TuiSnapshot snapshot;
            snapshot.currentTick = tickRef_;
            snapshot.scheduler.policyName = "RoundRobin";
            snapshot.sequence = static_cast<std::uint64_t>(captureCalls_);
            return Result<TuiSnapshot>::ok(std::move(snapshot));
        }

        void setCaptureError(ErrorCode code)
        {
            captureError_ = code;
        }

        [[nodiscard]] std::size_t captureCalls() const noexcept
        {
            return captureCalls_;
        }

        private:
        const Tick &tickRef_;
        mutable std::size_t captureCalls_ = 0;
        ErrorCode captureError_ = ErrorCode::Ok;
    };

} // namespace

TEST(TuiControllerTest, TickCommandAdvancesKernelAndAppendsHistory)
{
    Tick kernelTick = 0;
    std::size_t tickCalls = 0;
    std::size_t lastStep = 0;

    FakeKernelReadModel readModel(kernelTick);
    TuiController controller(readModel, [&kernelTick, &tickCalls, &lastStep](std::size_t step) {
        ++tickCalls;
        lastStep = step;
        kernelTick += step;
        return Result<void>::ok();
    });

    EXPECT_EQ(controller.historySize(), 1u);
    EXPECT_EQ(controller.current().currentTick, 0u);

    TuiCommand tickCommand;
    tickCommand.kind = TuiCommandKind::Tick;
    tickCommand.step = 3;

    auto result = controller.dispatch(tickCommand);

    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(tickCalls, 1u);
    EXPECT_EQ(lastStep, 3u);
    EXPECT_EQ(kernelTick, 3u);
    EXPECT_EQ(controller.current().currentTick, 3u);
    EXPECT_EQ(controller.historySize(), 2u);
    EXPECT_EQ(controller.state(), TuiControllerState::Idle);
}

TEST(TuiControllerTest, SeekBackwardAndForwardNavigatesHistoryOnly)
{
    Tick kernelTick = 0;
    FakeKernelReadModel readModel(kernelTick);

    TuiController controller(readModel, [&kernelTick](std::size_t step) {
        kernelTick += step;
        return Result<void>::ok();
    });

    TuiCommand tickCommand;
    tickCommand.kind = TuiCommandKind::Tick;
    tickCommand.step = 1;

    ASSERT_TRUE(controller.dispatch(tickCommand).isOk());
    ASSERT_TRUE(controller.dispatch(tickCommand).isOk());
    ASSERT_TRUE(controller.dispatch(tickCommand).isOk());
    ASSERT_EQ(controller.current().currentTick, 3u);

    TuiCommand back;
    back.kind = TuiCommandKind::SeekBackward;
    back.step = 2;
    ASSERT_TRUE(controller.dispatch(back).isOk());

    EXPECT_EQ(controller.current().currentTick, 1u);

    TuiCommand forward;
    forward.kind = TuiCommandKind::SeekForward;
    forward.step = 1;
    ASSERT_TRUE(controller.dispatch(forward).isOk());

    EXPECT_EQ(controller.current().currentTick, 2u);
    EXPECT_EQ(kernelTick, 3u);
}

TEST(TuiControllerTest, AutoplayAdvancesByIntervalAndStride)
{
    Tick kernelTick = 0;
    std::size_t tickCalls = 0;

    FakeKernelReadModel readModel(kernelTick);
    TuiController controller(readModel, [&kernelTick, &tickCalls](std::size_t step) {
        ++tickCalls;
        kernelTick += step;
        return Result<void>::ok();
    });

    TuiCommand start;
    start.kind = TuiCommandKind::AutoPlayStart;
    start.step = 2;
    start.intervalMs = 50;
    ASSERT_TRUE(controller.dispatch(start).isOk());
    ASSERT_EQ(controller.state(), TuiControllerState::Playing);

    ASSERT_TRUE(controller.advanceAutoplay(49).isOk());
    EXPECT_EQ(kernelTick, 0u);
    EXPECT_EQ(tickCalls, 0u);

    ASSERT_TRUE(controller.advanceAutoplay(1).isOk());
    EXPECT_EQ(kernelTick, 2u);
    EXPECT_EQ(tickCalls, 1u);

    ASSERT_TRUE(controller.advanceAutoplay(120).isOk());
    EXPECT_EQ(kernelTick, 6u);
    EXPECT_EQ(tickCalls, 3u);

    TuiCommand pause;
    pause.kind = TuiCommandKind::Pause;
    ASSERT_TRUE(controller.dispatch(pause).isOk());
    ASSERT_EQ(controller.state(), TuiControllerState::Paused);

    ASSERT_TRUE(controller.advanceAutoplay(200).isOk());
    EXPECT_EQ(kernelTick, 6u);
    EXPECT_EQ(tickCalls, 3u);
}

TEST(TuiControllerTest, AutoPlayStopReturnsToIdle)
{
    Tick kernelTick = 0;
    FakeKernelReadModel readModel(kernelTick);
    TuiController controller(readModel, [&kernelTick](std::size_t step) {
        kernelTick += step;
        return Result<void>::ok();
    });

    TuiCommand start;
    start.kind = TuiCommandKind::AutoPlayStart;
    start.step = 1;
    start.intervalMs = 10;
    ASSERT_TRUE(controller.dispatch(start).isOk());
    ASSERT_EQ(controller.state(), TuiControllerState::Playing);

    TuiCommand stop;
    stop.kind = TuiCommandKind::AutoPlayStop;
    ASSERT_TRUE(controller.dispatch(stop).isOk());
    EXPECT_EQ(controller.state(), TuiControllerState::Idle);
}

TEST(TuiControllerTest, InvalidCommandPayloadReturnsInvalidArgument)
{
    Tick kernelTick = 0;
    FakeKernelReadModel readModel(kernelTick);
    TuiController controller(readModel, [&kernelTick](std::size_t step) {
        kernelTick += step;
        return Result<void>::ok();
    });

    TuiCommand invalidTick;
    invalidTick.kind = TuiCommandKind::Tick;
    invalidTick.step = 0;

    auto result = controller.dispatch(invalidTick);

    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidArgument);
}

TEST(TuiControllerTest, TickPropagatesReadModelError)
{
    Tick kernelTick = 0;
    FakeKernelReadModel readModel(kernelTick);
    TuiController controller(readModel, [&kernelTick](std::size_t step) {
        kernelTick += step;
        return Result<void>::ok();
    });

    readModel.setCaptureError(ErrorCode::InvalidState);

    TuiCommand tickCommand;
    tickCommand.kind = TuiCommandKind::Tick;
    tickCommand.step = 1;

    auto result = controller.dispatch(tickCommand);

    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidState);
}

TEST(TuiControllerTest, AutoplayPausesWhenKernelReturnsNotFound)
{
    // Regression: before the fix, runForTicks swallowed NotFound and returned
    // ok(), so advanceAutoplay never saw an error and kept looping forever.
    Tick kernelTick = 0;
    int tickCalls = 0;
    FakeKernelReadModel readModel(kernelTick);
    TuiController controller(readModel, [&](std::size_t) {
        ++tickCalls;
        return Result<void>::error(ErrorCode::NotFound);
    });

    TuiCommand start;
    start.kind = TuiCommandKind::AutoPlayStart;
    start.step = 1;
    start.intervalMs = 100;
    ASSERT_TRUE(controller.dispatch(start).isOk());
    EXPECT_EQ(controller.state(), TuiControllerState::Playing);

    (void)controller.advanceAutoplay(250);

    // Controller must have noticed the idle kernel and stopped.
    EXPECT_EQ(controller.state(), TuiControllerState::Paused);
    // The tick callback should have been invoked exactly once, not multiple times.
    EXPECT_EQ(tickCalls, 1);
}

TEST(TuiControllerTest, AutoplayTickCallCountStopsOnFirstIdleSignal)
{
    Tick kernelTick = 0;
    int tickCalls = 0;
    FakeKernelReadModel readModel(kernelTick);
    TuiController controller(readModel, [&](std::size_t) {
        ++tickCalls;
        return Result<void>::error(ErrorCode::NotFound);
    });

    TuiCommand start;
    start.kind = TuiCommandKind::AutoPlayStart;
    start.step = 1;
    start.intervalMs = 100;
    ASSERT_TRUE(controller.dispatch(start).isOk());

    (void)controller.advanceAutoplay(500);

    EXPECT_EQ(tickCalls, 1) << "Should stop after first NotFound, not loop 5 times";
}

// Manual Tick on empty kernel (Bug 3 regression)
TEST(TuiControllerTest, TickCommandOnEmptyKernelReturnsOk)
{
    // Regression: before the fix, dispatching Tick when the kernel had no
    // processes propagated ErrorCode::NotFound back to the caller, causing the
    // TUI to surface a spurious error each time the user clicked "tick" after
    // all processes had finished.
    Tick kernelTick = 0;
    FakeKernelReadModel readModel(kernelTick);
    TuiController controller(readModel, [](std::size_t) { return Result<void>::error(ErrorCode::NotFound); });

    TuiCommand tickCommand;
    tickCommand.kind = TuiCommandKind::Tick;
    tickCommand.step = 1;

    auto result = controller.dispatch(tickCommand);
    EXPECT_TRUE(result.isOk()) << "Tick on empty kernel must not surface NotFound to the caller";
}

TEST(TuiControllerTest, TickCommandOnEmptyKernelStillCapturesSnapshot)
{
    // Even when the kernel has nothing to do, the current snapshot should be
    // refreshed so that the TUI continues to reflect the up-to-date state.
    Tick kernelTick = 0;
    FakeKernelReadModel readModel(kernelTick);
    std::size_t capturesBefore = 0;

    TuiController controller(readModel, [](std::size_t) { return Result<void>::error(ErrorCode::NotFound); });

    capturesBefore = readModel.captureCalls();

    TuiCommand tickCommand;
    tickCommand.kind = TuiCommandKind::Tick;
    tickCommand.step = 1;

    ASSERT_TRUE(controller.dispatch(tickCommand).isOk());
    EXPECT_GT(readModel.captureCalls(), capturesBefore);
}

TEST(TuiControllerTest, TickCommandPropagatesNonNotFoundErrors)
{
    // NotFound is swallowed (empty kernel), but other errors (e.g.
    // InvalidState) must still be propagated so real failures are visible.
    Tick kernelTick = 0;
    FakeKernelReadModel readModel(kernelTick);
    TuiController controller(readModel, [](std::size_t) { return Result<void>::error(ErrorCode::InvalidState); });

    TuiCommand tickCommand;
    tickCommand.kind = TuiCommandKind::Tick;
    tickCommand.step = 1;

    auto result = controller.dispatch(tickCommand);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidState);
}

TEST(TuiControllerTest, RepeatedTicksOnEmptyKernelNeverError)
{
    // Simulates the user repeatedly pressing [t] after all processes finish.
    Tick kernelTick = 0;
    FakeKernelReadModel readModel(kernelTick);
    int tickCalls = 0;
    TuiController controller(readModel, [&tickCalls](std::size_t) {
        ++tickCalls;
        return Result<void>::error(ErrorCode::NotFound);
    });

    TuiCommand tickCommand;
    tickCommand.kind = TuiCommandKind::Tick;
    tickCommand.step = 1;

    for (int i = 0; i < 5; ++i)
    {
        EXPECT_TRUE(controller.dispatch(tickCommand).isOk()) << "tick " << i + 1 << " returned error";
    }
    EXPECT_EQ(tickCalls, 5);
}

TEST(TuiControllerTest, AutoplayStillPausesOnEmptyKernelAfterTickFix)
{
    // Autoplay must still pause when the kernel returns NotFound, even after
    // the fix that lets manual Tick silently succeed on an empty kernel.
    Tick kernelTick = 0;
    FakeKernelReadModel readModel(kernelTick);
    TuiController controller(readModel, [](std::size_t) { return Result<void>::error(ErrorCode::NotFound); });

    TuiCommand start;
    start.kind = TuiCommandKind::AutoPlayStart;
    start.step = 1;
    start.intervalMs = 100;
    ASSERT_TRUE(controller.dispatch(start).isOk());
    EXPECT_EQ(controller.state(), TuiControllerState::Playing);

    (void)controller.advanceAutoplay(300);

    EXPECT_EQ(controller.state(), TuiControllerState::Paused) << "Autoplay must pause when the kernel is exhausted";
}
