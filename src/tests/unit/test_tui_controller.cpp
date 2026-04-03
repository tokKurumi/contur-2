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
    TuiController controller(
        readModel,
        [&kernelTick, &tickCalls, &lastStep](std::size_t step) {
            ++tickCalls;
            lastStep = step;
            kernelTick += step;
            return Result<void>::ok();
        }
    );

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

    TuiController controller(
        readModel,
        [&kernelTick](std::size_t step) {
            kernelTick += step;
            return Result<void>::ok();
        }
    );

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
    TuiController controller(
        readModel,
        [&kernelTick, &tickCalls](std::size_t step) {
            ++tickCalls;
            kernelTick += step;
            return Result<void>::ok();
        }
    );

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
    TuiController controller(
        readModel,
        [&kernelTick](std::size_t step) {
            kernelTick += step;
            return Result<void>::ok();
        }
    );

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
    TuiController controller(
        readModel,
        [&kernelTick](std::size_t step) {
            kernelTick += step;
            return Result<void>::ok();
        }
    );

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
    TuiController controller(
        readModel,
        [&kernelTick](std::size_t step) {
            kernelTick += step;
            return Result<void>::ok();
        }
    );

    readModel.setCaptureError(ErrorCode::InvalidState);

    TuiCommand tickCommand;
    tickCommand.kind = TuiCommandKind::Tick;
    tickCommand.step = 1;

    auto result = controller.dispatch(tickCommand);

    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidState);
}
