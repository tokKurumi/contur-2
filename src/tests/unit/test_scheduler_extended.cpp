/// @file test_scheduler_extended.cpp
/// @brief Extended unit tests for Scheduler — multi-process queues, lane
///        management, policy replacement, and blocked-queue transitions.

#include <algorithm>

#include <gtest/gtest.h>

#include "contur/core/clock.h"
#include "contur/process/pcb.h"
#include "contur/process/state.h"
#include "contur/scheduling/fcfs_policy.h"
#include "contur/scheduling/round_robin_policy.h"
#include "contur/scheduling/scheduler.h"
#include "contur/tracing/null_tracer.h"

using namespace contur;

namespace {

    struct Fix
    {
        SimulationClock clock{};
        NullTracer tracer{clock};
        Scheduler scheduler{std::make_unique<FcfsPolicy>(), tracer};
    };

    bool contains(const std::vector<ProcessId> &v, ProcessId pid)
    {
        return std::find(v.begin(), v.end(), pid) != v.end();
    }

} // namespace

// ---------------------------------------------------------------------------
// Empty-queue behaviour
// ---------------------------------------------------------------------------

TEST(SchedulerExtTest, SelectNextFromEmptyQueueReturnsError)
{
    Fix f;
    auto result = f.scheduler.selectNext(f.clock);
    ASSERT_TRUE(result.isError());
}

// ---------------------------------------------------------------------------
// Multi-process ready queue
// ---------------------------------------------------------------------------

TEST(SchedulerExtTest, MultipleEnqueuesAllAppearInSnapshot)
{
    Fix f;
    PCB p1(1, "p1"), p2(2, "p2"), p3(3, "p3");
    ASSERT_TRUE(f.scheduler.enqueue(p1, 0).isOk());
    ASSERT_TRUE(f.scheduler.enqueue(p2, 1).isOk());
    ASSERT_TRUE(f.scheduler.enqueue(p3, 2).isOk());

    auto snap = f.scheduler.getQueueSnapshot();
    EXPECT_EQ(snap.size(), 3u);
    EXPECT_TRUE(contains(snap, 1));
    EXPECT_TRUE(contains(snap, 2));
    EXPECT_TRUE(contains(snap, 3));
}

TEST(SchedulerExtTest, DequeueRemovesExactProcessFromReadyQueue)
{
    Fix f;
    PCB p1(1, "p1"), p2(2, "p2");
    ASSERT_TRUE(f.scheduler.enqueue(p1, 0).isOk());
    ASSERT_TRUE(f.scheduler.enqueue(p2, 1).isOk());

    ASSERT_TRUE(f.scheduler.dequeue(1).isOk());

    auto snap = f.scheduler.getQueueSnapshot();
    EXPECT_EQ(snap.size(), 1u);
    EXPECT_EQ(snap.front(), 2u);
}

TEST(SchedulerExtTest, DequeueNonExistentPidReturnsError)
{
    Fix f;
    ASSERT_TRUE(f.scheduler.dequeue(999).isError());
}

TEST(SchedulerExtTest, FcfsSelectsThreeProcessesInArrivalOrder)
{
    Fix f;
    PCB p1(1, "p1", Priority{}, 10);
    PCB p2(2, "p2", Priority{}, 5);
    PCB p3(3, "p3", Priority{}, 20);

    ASSERT_TRUE(f.scheduler.enqueue(p1, 0).isOk());
    ASSERT_TRUE(f.scheduler.enqueue(p2, 0).isOk());
    ASSERT_TRUE(f.scheduler.enqueue(p3, 0).isOk());

    // 1st pick: p2 (earliest arrival = 5)
    auto sel1 = f.scheduler.selectNext(f.clock);
    ASSERT_TRUE(sel1.isOk());
    EXPECT_EQ(sel1.value(), 2u);
    ASSERT_TRUE(f.scheduler.terminate(2, 1).isOk());

    // 2nd pick: p1 (arrival = 10)
    auto sel2 = f.scheduler.selectNext(f.clock);
    ASSERT_TRUE(sel2.isOk());
    EXPECT_EQ(sel2.value(), 1u);
    ASSERT_TRUE(f.scheduler.terminate(1, 2).isOk());

    // 3rd pick: p3 (arrival = 20)
    auto sel3 = f.scheduler.selectNext(f.clock);
    ASSERT_TRUE(sel3.isOk());
    EXPECT_EQ(sel3.value(), 3u);
}

// ---------------------------------------------------------------------------
// Blocked queue
// ---------------------------------------------------------------------------

TEST(SchedulerExtTest, BlockedQueueSnapshotAfterBlockRunning)
{
    Fix f;
    PCB p1(1, "p1");
    ASSERT_TRUE(f.scheduler.enqueue(p1, 0).isOk());
    ASSERT_TRUE(f.scheduler.selectNext(f.clock).isOk());

    ASSERT_TRUE(f.scheduler.blockRunning(1).isOk());

    auto blocked = f.scheduler.getBlockedSnapshot();
    ASSERT_EQ(blocked.size(), 1u);
    EXPECT_EQ(blocked.front(), 1u);
    EXPECT_TRUE(f.scheduler.runningProcesses().empty());
}

TEST(SchedulerExtTest, BlockProcessByPidFromRunningState)
{
    Fix f;
    PCB p1(1, "p1"), p2(2, "p2");
    ASSERT_TRUE(f.scheduler.enqueue(p1, 0).isOk());
    ASSERT_TRUE(f.scheduler.enqueue(p2, 0).isOk());

    // Select p1 first so it is Running, then block it via blockProcess
    ASSERT_TRUE(f.scheduler.selectNext(f.clock).isOk());
    ASSERT_TRUE(f.scheduler.blockProcess(1, 1).isOk());

    // p1 moved to blocked; p2 still in ready queue
    auto ready = f.scheduler.getQueueSnapshot();
    EXPECT_EQ(ready.size(), 1u);
    EXPECT_EQ(ready.front(), 2u);

    auto blocked = f.scheduler.getBlockedSnapshot();
    EXPECT_EQ(blocked.size(), 1u);
    EXPECT_EQ(blocked.front(), 1u);
}

TEST(SchedulerExtTest, UnblockMovesProcessBackToReady)
{
    Fix f;
    PCB p1(1, "p1");
    ASSERT_TRUE(f.scheduler.enqueue(p1, 0).isOk());
    ASSERT_TRUE(f.scheduler.selectNext(f.clock).isOk());
    ASSERT_TRUE(f.scheduler.blockRunning(1).isOk());

    ASSERT_TRUE(f.scheduler.unblock(1, 5).isOk());

    EXPECT_TRUE(f.scheduler.getBlockedSnapshot().empty());
    auto ready = f.scheduler.getQueueSnapshot();
    EXPECT_EQ(ready.size(), 1u);
    EXPECT_EQ(ready.front(), 1u);
}

TEST(SchedulerExtTest, UnblockNonExistentPidReturnsError)
{
    Fix f;
    ASSERT_TRUE(f.scheduler.unblock(999, 5).isError());
}

// ---------------------------------------------------------------------------
// Termination edge cases
// ---------------------------------------------------------------------------

TEST(SchedulerExtTest, TerminateUnknownPidReturnsError)
{
    Fix f;
    ASSERT_TRUE(f.scheduler.terminate(999, 10).isError());
}

TEST(SchedulerExtTest, DequeueReadyProcessRemovesItFromQueue)
{
    // terminate() operates on Running processes; dequeue() removes any tracked process
    Fix f;
    PCB p1(1, "p1"), p2(2, "p2");
    ASSERT_TRUE(f.scheduler.enqueue(p1, 0).isOk());
    ASSERT_TRUE(f.scheduler.enqueue(p2, 0).isOk());

    ASSERT_TRUE(f.scheduler.dequeue(1).isOk());

    auto snap = f.scheduler.getQueueSnapshot();
    EXPECT_EQ(snap.size(), 1u);
    EXPECT_EQ(snap.front(), 2u);
}

TEST(SchedulerExtTest, TerminateRunningProcessClearsRunningList)
{
    Fix f;
    PCB p1(1, "p1");
    ASSERT_TRUE(f.scheduler.enqueue(p1, 0).isOk());
    ASSERT_TRUE(f.scheduler.selectNext(f.clock).isOk());
    EXPECT_EQ(f.scheduler.runningProcesses().size(), 1u);

    ASSERT_TRUE(f.scheduler.terminate(1, 5).isOk());

    EXPECT_TRUE(f.scheduler.runningProcesses().empty());
    EXPECT_TRUE(f.scheduler.getQueueSnapshot().empty());
}

// ---------------------------------------------------------------------------
// Policy replacement
// ---------------------------------------------------------------------------

TEST(SchedulerExtTest, SetPolicyChangesPolicyName)
{
    Fix f;
    EXPECT_EQ(f.scheduler.policyName(), "FCFS");

    ASSERT_TRUE(f.scheduler.setPolicy(std::make_unique<RoundRobinPolicy>(4)).isOk());
    EXPECT_EQ(f.scheduler.policyName(), "RoundRobin");
}

TEST(SchedulerExtTest, SetPolicyRejectsNullptr)
{
    Fix f;
    auto result = f.scheduler.setPolicy(nullptr);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidState);
}

// ---------------------------------------------------------------------------
// Lane management
// ---------------------------------------------------------------------------

TEST(SchedulerExtTest, ConfigureLanesZeroIsInvalidArgument)
{
    Fix f;
    auto result = f.scheduler.configureLanes(0);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidArgument);
}

TEST(SchedulerExtTest, ConfigureMultipleLanesAndEnqueuePerLane)
{
    SimulationClock clock;
    NullTracer tracer(clock);
    Scheduler scheduler(std::make_unique<FcfsPolicy>(), tracer);

    ASSERT_TRUE(scheduler.configureLanes(3).isOk());
    EXPECT_EQ(scheduler.laneCount(), 3u);

    PCB p1(1, "p1"), p2(2, "p2"), p3(3, "p3");
    ASSERT_TRUE(scheduler.enqueueToLane(p1, 0, 0).isOk());
    ASSERT_TRUE(scheduler.enqueueToLane(p2, 1, 0).isOk());
    ASSERT_TRUE(scheduler.enqueueToLane(p3, 2, 0).isOk());

    auto perLane = scheduler.getPerLaneQueueSnapshot();
    ASSERT_EQ(perLane.size(), 3u);
    EXPECT_EQ(perLane[0].size(), 1u);
    EXPECT_EQ(perLane[1].size(), 1u);
    EXPECT_EQ(perLane[2].size(), 1u);
    EXPECT_EQ(perLane[0].front(), 1u);
    EXPECT_EQ(perLane[1].front(), 2u);
    EXPECT_EQ(perLane[2].front(), 3u);
}

TEST(SchedulerExtTest, SelectNextForLanePicsFromCorrectLane)
{
    SimulationClock clock;
    NullTracer tracer(clock);
    Scheduler scheduler(std::make_unique<FcfsPolicy>(), tracer);

    ASSERT_TRUE(scheduler.configureLanes(2).isOk());

    PCB p1(1, "p1"), p2(2, "p2");
    ASSERT_TRUE(scheduler.enqueueToLane(p1, 0, 0).isOk());
    ASSERT_TRUE(scheduler.enqueueToLane(p2, 1, 0).isOk());

    auto sel0 = scheduler.selectNextForLane(0, clock);
    ASSERT_TRUE(sel0.isOk());
    EXPECT_EQ(sel0.value(), 1u);

    auto sel1 = scheduler.selectNextForLane(1, clock);
    ASSERT_TRUE(sel1.isOk());
    EXPECT_EQ(sel1.value(), 2u);
}

TEST(SchedulerExtTest, StealNextForLanePullsFromAnotherLane)
{
    SimulationClock clock;
    NullTracer tracer(clock);
    Scheduler scheduler(std::make_unique<FcfsPolicy>(), tracer);

    ASSERT_TRUE(scheduler.configureLanes(2).isOk());

    PCB p1(1, "p1"), p2(2, "p2");
    ASSERT_TRUE(scheduler.enqueueToLane(p1, 0, 0).isOk());
    ASSERT_TRUE(scheduler.enqueueToLane(p2, 0, 0).isOk());

    // Lane 1 is empty — steal from lane 0
    auto stolen = scheduler.stealNextForLane(1, clock);
    ASSERT_TRUE(stolen.isOk());

    // A process now runs on lane 1
    auto running = scheduler.runningProcesses();
    EXPECT_FALSE(running.empty());
}

TEST(SchedulerExtTest, RunningProcessesReflectsActiveSelections)
{
    Fix f;
    PCB p1(1, "p1");
    ASSERT_TRUE(f.scheduler.enqueue(p1, 0).isOk());
    ASSERT_TRUE(f.scheduler.selectNext(f.clock).isOk());

    auto running = f.scheduler.runningProcesses();
    ASSERT_EQ(running.size(), 1u);
    EXPECT_EQ(running.front(), 1u);
}
