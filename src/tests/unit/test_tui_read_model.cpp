/// @file test_tui_read_model.cpp
/// @brief Unit tests for KernelReadModel adapter.

#include <utility>

#include <optional>

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
    kernelSnapshot.readyQueue = {1, 2};
    kernelSnapshot.blockedQueue = {7};
    kernelSnapshot.perLaneReadyQueues = {{1}, {2}};
    kernelSnapshot.policyName = "RoundRobin";

    kernelSnapshot.processes = {
        KernelProcessSnapshot{
            .id = 1,
            .name = "alpha",
            .state = ProcessState::Ready,
            .basePriority = PriorityLevel::Normal,
            .effectivePriority = PriorityLevel::AboveNormal,
            .nice = -2,
            .cpuTime = 13,
            .laneIndex = 0,
        },
        KernelProcessSnapshot{
            .id = 2,
            .name = "beta",
            .state = ProcessState::Blocked,
            .basePriority = PriorityLevel::Low,
            .effectivePriority = PriorityLevel::Low,
            .nice = 3,
            .cpuTime = 4,
            .laneIndex = std::nullopt,
        },
    };

    kernelSnapshot.totalVirtualSlots = 64;
    kernelSnapshot.freeVirtualSlots = 40;
    kernelSnapshot.totalFrames = 128;
    kernelSnapshot.freeFrames = 96;
    kernelSnapshot.frameOwners = {1, std::nullopt, 2};

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
    EXPECT_EQ(snapshot.scheduler.readyQueue, kernelSnapshot.readyQueue);
    EXPECT_EQ(snapshot.scheduler.blockedQueue, kernelSnapshot.blockedQueue);
    EXPECT_EQ(snapshot.scheduler.perLaneReadyQueues, kernelSnapshot.perLaneReadyQueues);
    EXPECT_EQ(snapshot.scheduler.policyName, "RoundRobin");

    ASSERT_EQ(snapshot.processes.size(), 2u);
    EXPECT_EQ(snapshot.processes[0].id, 1u);
    EXPECT_EQ(snapshot.processes[0].name, "alpha");
    EXPECT_EQ(snapshot.processes[0].state, ProcessState::Ready);
    EXPECT_EQ(snapshot.processes[0].basePriority, PriorityLevel::Normal);
    EXPECT_EQ(snapshot.processes[0].effectivePriority, PriorityLevel::AboveNormal);
    EXPECT_EQ(snapshot.processes[0].nice, -2);
    EXPECT_EQ(snapshot.processes[0].cpuTime, 13u);
    ASSERT_TRUE(snapshot.processes[0].laneIndex.has_value());
    EXPECT_EQ(snapshot.processes[0].laneIndex.value(), 0u);

    EXPECT_EQ(snapshot.processes[1].id, 2u);
    EXPECT_FALSE(snapshot.processes[1].laneIndex.has_value());

    EXPECT_EQ(snapshot.memory.totalVirtualSlots, 64u);
    EXPECT_EQ(snapshot.memory.freeVirtualSlots, 40u);
    ASSERT_TRUE(snapshot.memory.totalFrames.has_value());
    ASSERT_TRUE(snapshot.memory.freeFrames.has_value());
    EXPECT_EQ(snapshot.memory.totalFrames.value(), 128u);
    EXPECT_EQ(snapshot.memory.freeFrames.value(), 96u);
    ASSERT_EQ(snapshot.memory.frameOwners.size(), 3u);
    ASSERT_TRUE(snapshot.memory.frameOwners[1] == std::nullopt);
    EXPECT_GT(snapshot.sequence, 0u);
    EXPECT_EQ(snapshot.epoch, 42u);
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

TEST(KernelReadModelTest, CaptureSnapshotSequenceIsMonotonic)
{
    KernelSnapshot kernelSnapshot;
    kernelSnapshot.currentTick = 8;

    FakeKernelDiagnostics diagnostics(kernelSnapshot);
    KernelReadModel readModel(diagnostics);

    auto first = readModel.captureSnapshot();
    auto second = readModel.captureSnapshot();

    ASSERT_TRUE(first.isOk());
    ASSERT_TRUE(second.isOk());
    EXPECT_LT(first.value().sequence, second.value().sequence);
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
