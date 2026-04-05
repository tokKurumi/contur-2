/// @file test_tui_read_model_extended.cpp
/// @brief Extended unit tests for KernelReadModel — epoch, sequence
///        monotonicity, process priority fields, frame owner mapping,
///        empty memory fields, repeated captures and error propagation.

#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "contur/core/error.h"

#include "contur/kernel/i_kernel_diagnostics.h"
#include "contur/tui/i_kernel_read_model.h"

using namespace contur;

namespace {

    class FakeDiagnostics final : public IKernelDiagnostics
    {
        public:
        KernelSnapshot snapshot;
        ErrorCode captureError = ErrorCode::Ok;
        mutable std::size_t callCount = 0;

        [[nodiscard]] Result<KernelDiagnosticsSnapshot> captureSnapshot() const override
        {
            ++callCount;
            if (captureError != ErrorCode::Ok)
            {
                return Result<KernelDiagnosticsSnapshot>::error(captureError);
            }
            KernelDiagnosticsSnapshot out;
            out.kernel = snapshot;
            return Result<KernelDiagnosticsSnapshot>::ok(std::move(out));
        }
    };

} // namespace

// Epoch mirrors kernel tick
TEST(KernelReadModelExtTest, EpochMatchesCurrentTick)
{
    FakeDiagnostics diag;
    diag.snapshot.currentTick = 77;
    KernelReadModel model(diag);

    auto r = model.captureSnapshot();
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value().epoch, 77u);
}

// Sequence is monotonically increasing
TEST(KernelReadModelExtTest, SequenceMonotonicallyIncreasesAcrossCaptures)
{
    FakeDiagnostics diag;
    KernelReadModel model(diag);

    std::uint64_t prev = 0;
    for (int i = 0; i < 5; ++i)
    {
        auto r = model.captureSnapshot();
        ASSERT_TRUE(r.isOk()) << "capture " << i;
        EXPECT_GT(r.value().sequence, prev) << "capture " << i;
        prev = r.value().sequence;
    }
}

// Process priority fields
TEST(KernelReadModelExtTest, ProcessPriorityFieldsAreMappedCorrectly)
{
    FakeDiagnostics diag;
    diag.snapshot.processes.push_back(
        KernelProcessSnapshot{
            .id = 7,
            .name = "prio-proc",
            .state = ProcessState::Ready,
            .basePriority = PriorityLevel::High,
            .effectivePriority = PriorityLevel::Realtime,
            .nice = -5,
            .cpuTime = 42,
            .laneIndex = std::nullopt,
        }
    );

    KernelReadModel model(diag);
    auto r = model.captureSnapshot();
    ASSERT_TRUE(r.isOk());
    ASSERT_EQ(r.value().processes.size(), 1u);

    const auto &p = r.value().processes[0];
    EXPECT_EQ(p.id, 7u);
    EXPECT_EQ(p.name, "prio-proc");
    EXPECT_EQ(p.basePriority, PriorityLevel::High);
    EXPECT_EQ(p.effectivePriority, PriorityLevel::Realtime);
    EXPECT_EQ(p.nice, -5);
    EXPECT_EQ(p.cpuTime, 42u);
    EXPECT_FALSE(p.laneIndex.has_value());
}

// Frame owner mapping
TEST(KernelReadModelExtTest, FrameOwnersMappedToTuiModel)
{
    FakeDiagnostics diag;
    diag.snapshot.totalFrames = 4;
    diag.snapshot.freeFrames = 2;
    diag.snapshot.frameOwners = {ProcessId{1}, std::nullopt, ProcessId{2}, std::nullopt};

    KernelReadModel model(diag);
    auto r = model.captureSnapshot();
    ASSERT_TRUE(r.isOk());

    const auto &mem = r.value().memory;
    ASSERT_EQ(mem.frameOwners.size(), 4u);
    ASSERT_TRUE(mem.frameOwners[0].has_value());
    EXPECT_EQ(mem.frameOwners[0].value(), 1u);
    EXPECT_FALSE(mem.frameOwners[1].has_value());
    ASSERT_TRUE(mem.frameOwners[2].has_value());
    EXPECT_EQ(mem.frameOwners[2].value(), 2u);
    EXPECT_FALSE(mem.frameOwners[3].has_value());
}

// Empty memory fields when kernel has no physical memory info
TEST(KernelReadModelExtTest, EmptyMemoryFieldsWhenFrameInfoAbsent)
{
    FakeDiagnostics diag;
    // totalFrames / freeFrames left as std::nullopt in KernelSnapshot

    KernelReadModel model(diag);
    auto r = model.captureSnapshot();
    ASSERT_TRUE(r.isOk());

    const auto &mem = r.value().memory;
    EXPECT_FALSE(mem.totalFrames.has_value());
    EXPECT_FALSE(mem.freeFrames.has_value());
    EXPECT_TRUE(mem.frameOwners.empty());
}

// Process count consistency
TEST(KernelReadModelExtTest, ProcessCountMatchesProcessListSize)
{
    FakeDiagnostics diag;
    diag.snapshot.processCount = 3;
    for (ProcessId pid = 1; pid <= 3; ++pid)
    {
        diag.snapshot.processes.push_back(
            KernelProcessSnapshot{
                .id = pid,
                .name = std::string("p") + std::to_string(pid),
                .state = ProcessState::Ready,
                .laneIndex = std::nullopt,
            }
        );
    }

    KernelReadModel model(diag);
    auto r = model.captureSnapshot();
    ASSERT_TRUE(r.isOk());

    EXPECT_EQ(r.value().processCount, 3u);
    EXPECT_EQ(r.value().processes.size(), 3u);
}

// Scheduler snapshot fields
TEST(KernelReadModelExtTest, SchedulerSnapshotFieldsMapped)
{
    FakeDiagnostics diag;
    diag.snapshot.policyName = "FCFS";
    diag.snapshot.readyCount = 4;
    diag.snapshot.blockedCount = 1;
    diag.snapshot.runningPids = {10};
    diag.snapshot.readyQueue = {1, 2, 3, 4};
    diag.snapshot.blockedQueue = {5};
    diag.snapshot.perLaneReadyQueues = {{1, 2}, {3, 4}};

    KernelReadModel model(diag);
    auto r = model.captureSnapshot();
    ASSERT_TRUE(r.isOk());

    const auto &sched = r.value().scheduler;
    EXPECT_EQ(sched.policyName, "FCFS");
    EXPECT_EQ(sched.readyCount, 4u);
    EXPECT_EQ(sched.blockedCount, 1u);
    EXPECT_EQ(sched.runningQueue, (std::vector<ProcessId>{10}));
    EXPECT_EQ(sched.readyQueue, (std::vector<ProcessId>{1, 2, 3, 4}));
    EXPECT_EQ(sched.blockedQueue, (std::vector<ProcessId>{5}));
    ASSERT_EQ(sched.perLaneReadyQueues.size(), 2u);
    EXPECT_EQ(sched.perLaneReadyQueues[0], (std::vector<ProcessId>{1, 2}));
    EXPECT_EQ(sched.perLaneReadyQueues[1], (std::vector<ProcessId>{3, 4}));
}

// Error propagation
TEST(KernelReadModelExtTest, NotFoundErrorFromDiagnosticsPropagates)
{
    FakeDiagnostics diag;
    diag.captureError = ErrorCode::NotFound;

    KernelReadModel model(diag);
    auto r = model.captureSnapshot();

    ASSERT_TRUE(r.isError());
    EXPECT_EQ(r.errorCode(), ErrorCode::NotFound);
}

TEST(KernelReadModelExtTest, OutOfMemoryErrorFromDiagnosticsPropagates)
{
    FakeDiagnostics diag;
    diag.captureError = ErrorCode::OutOfMemory;

    KernelReadModel model(diag);
    auto r = model.captureSnapshot();

    ASSERT_TRUE(r.isError());
    EXPECT_EQ(r.errorCode(), ErrorCode::OutOfMemory);
}
