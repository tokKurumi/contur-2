/// @file test_tui_read_model.cpp
/// @brief Unit tests for KernelReadModel adapter.

#include <utility>

#include <gtest/gtest.h>

#include "contur/core/error.h"
#include "contur/kernel/i_kernel_diagnostics.h"
#include "contur/tui/i_kernel_read_model.h"

using namespace contur;

namespace {

    class FakeKernelDiagnostics final : public IKernelDiagnostics
    {
        public:
        explicit FakeKernelDiagnostics(KernelSnapshot snapshot, ErrorCode captureError = ErrorCode::Ok)
            : snapshot_(std::move(snapshot))
            , captureError_(captureError)
        {}

        [[nodiscard]] Result<KernelDiagnosticsSnapshot> captureSnapshot() const override
        {
            if (captureError_ != ErrorCode::Ok)
            {
                return Result<KernelDiagnosticsSnapshot>::error(captureError_);
            }

            KernelDiagnosticsSnapshot out;
            out.kernel = snapshot_;
            return Result<KernelDiagnosticsSnapshot>::ok(std::move(out));
        }

        private:
        KernelSnapshot snapshot_;
        ErrorCode captureError_ = ErrorCode::Ok;
    };

} // namespace

TEST(KernelReadModelTest, CaptureSnapshotMapsKernelFields)
{
    KernelSnapshot kernelSnapshot;
    kernelSnapshot.currentTick = 42;
    kernelSnapshot.processCount = 3;
    kernelSnapshot.readyCount = 2;
    kernelSnapshot.blockedCount = 1;
    kernelSnapshot.runningPids = {5, 9};
    kernelSnapshot.totalVirtualSlots = 64;
    kernelSnapshot.freeVirtualSlots = 40;

    FakeKernelDiagnostics diagnostics(kernelSnapshot);
    KernelReadModel readModel(diagnostics);

    auto captured = readModel.captureSnapshot();

    ASSERT_TRUE(captured.isOk());
    const TuiSnapshot &snapshot = captured.value();

    EXPECT_EQ(snapshot.currentTick, 42u);
    EXPECT_EQ(snapshot.processCount, 3u);
    EXPECT_EQ(snapshot.scheduler.readyCount, 2u);
    EXPECT_EQ(snapshot.scheduler.blockedCount, 1u);
    EXPECT_EQ(snapshot.scheduler.runningQueue, kernelSnapshot.runningPids);
    EXPECT_EQ(snapshot.memory.totalVirtualSlots, 64u);
    EXPECT_EQ(snapshot.memory.freeVirtualSlots, 40u);
}

TEST(KernelReadModelTest, CaptureSnapshotKeepsUnavailableCollectionsEmpty)
{
    KernelSnapshot kernelSnapshot;
    kernelSnapshot.currentTick = 1;
    kernelSnapshot.processCount = 0;

    FakeKernelDiagnostics diagnostics(kernelSnapshot);
    KernelReadModel readModel(diagnostics);

    auto captured = readModel.captureSnapshot();

    ASSERT_TRUE(captured.isOk());
    const TuiSnapshot &snapshot = captured.value();

    EXPECT_TRUE(snapshot.processes.empty());
    EXPECT_TRUE(snapshot.scheduler.readyQueue.empty());
    EXPECT_TRUE(snapshot.scheduler.blockedQueue.empty());
    EXPECT_TRUE(snapshot.scheduler.perLaneReadyQueues.empty());
    EXPECT_TRUE(snapshot.memory.frameOwners.empty());
    EXPECT_FALSE(snapshot.memory.totalFrames.has_value());
    EXPECT_FALSE(snapshot.memory.freeFrames.has_value());
}

TEST(KernelReadModelTest, CaptureSnapshotPropagatesDiagnosticsError)
{
    KernelSnapshot kernelSnapshot;

    FakeKernelDiagnostics diagnostics(kernelSnapshot, ErrorCode::InvalidState);
    KernelReadModel readModel(diagnostics);

    auto captured = readModel.captureSnapshot();

    ASSERT_TRUE(captured.isError());
    EXPECT_EQ(captured.errorCode(), ErrorCode::InvalidState);
}
