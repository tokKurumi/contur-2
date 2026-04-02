/// @file test_scheduler.cpp
/// @brief Unit tests for scheduler queue/state transitions.

#include <gtest/gtest.h>

#include "contur/core/clock.h"

#include "contur/process/pcb.h"
#include "contur/process/state.h"
#include "contur/scheduling/fcfs_policy.h"
#include "contur/scheduling/scheduler.h"
#include "contur/tracing/null_tracer.h"

using namespace contur;

TEST(SchedulerTest, EnqueueAndSelectMovesProcessToRunning)
{
    SimulationClock clock;
    NullTracer tracer(clock);
    Scheduler scheduler(std::make_unique<FcfsPolicy>(), tracer);

    PCB p1(1, "p1");
    ASSERT_TRUE(scheduler.enqueue(p1, 1).isOk());

    auto selected = scheduler.selectNext(clock);
    ASSERT_TRUE(selected.isOk());
    EXPECT_EQ(selected.value(), 1u);
    auto running = scheduler.runningProcesses();
    ASSERT_EQ(running.size(), 1u);
    EXPECT_EQ(running.front(), 1u);
    EXPECT_EQ(p1.state(), ProcessState::Running);
}

TEST(SchedulerTest, BlockAndUnblockTransitions)
{
    SimulationClock clock;
    NullTracer tracer(clock);
    Scheduler scheduler(std::make_unique<FcfsPolicy>(), tracer);

    PCB p1(1, "p1");
    ASSERT_TRUE(scheduler.enqueue(p1, 0).isOk());
    ASSERT_TRUE(scheduler.selectNext(clock).isOk());

    ASSERT_TRUE(scheduler.blockRunning(5).isOk());
    EXPECT_TRUE(scheduler.runningProcesses().empty());
    EXPECT_EQ(p1.state(), ProcessState::Blocked);

    ASSERT_TRUE(scheduler.unblock(1, 7).isOk());
    EXPECT_EQ(p1.state(), ProcessState::Ready);
    ASSERT_EQ(scheduler.getQueueSnapshot().size(), 1u);
}

TEST(SchedulerTest, TerminateRunningProcess)
{
    SimulationClock clock;
    NullTracer tracer(clock);
    Scheduler scheduler(std::make_unique<FcfsPolicy>(), tracer);

    PCB p1(1, "p1");
    ASSERT_TRUE(scheduler.enqueue(p1, 0).isOk());
    ASSERT_TRUE(scheduler.selectNext(clock).isOk());

    ASSERT_TRUE(scheduler.terminate(1, 10).isOk());
    EXPECT_EQ(p1.state(), ProcessState::Terminated);
    EXPECT_TRUE(scheduler.runningProcesses().empty());
}

TEST(SchedulerTest, NullPolicyReturnsInvalidStateInsteadOfThrow)
{
    SimulationClock clock;
    NullTracer tracer(clock);
    Scheduler scheduler(nullptr, tracer);

    PCB p1(1, "p1");
    ASSERT_TRUE(scheduler.enqueue(p1, 0).isOk());

    auto selected = scheduler.selectNext(clock);

    ASSERT_TRUE(selected.isError());
    EXPECT_EQ(selected.errorCode(), ErrorCode::InvalidState);
    EXPECT_EQ(scheduler.policyName(), "Unconfigured");
}

TEST(SchedulerTest, SetPolicyRejectsNullWithInvalidState)
{
    SimulationClock clock;
    NullTracer tracer(clock);
    Scheduler scheduler(std::make_unique<FcfsPolicy>(), tracer);

    auto result = scheduler.setPolicy(nullptr);

    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidState);
}
