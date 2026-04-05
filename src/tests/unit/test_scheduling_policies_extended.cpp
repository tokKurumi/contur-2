/// @file test_scheduling_policies_extended.cpp
/// @brief Extended unit tests for all scheduling policies — covers edge cases,
///        empty queues, tie-breaking, time-slice expiry, and policy names.

#include <gtest/gtest.h>

#include "contur/core/clock.h"
#include "contur/process/pcb.h"
#include "contur/process/state.h"
#include "contur/scheduling/fcfs_policy.h"
#include "contur/scheduling/hrrn_policy.h"
#include "contur/scheduling/mlfq_policy.h"
#include "contur/scheduling/priority_policy.h"
#include "contur/scheduling/round_robin_policy.h"
#include "contur/scheduling/spn_policy.h"
#include "contur/scheduling/srt_policy.h"

using namespace contur;

// ---------------------------------------------------------------------------
// RoundRobinPolicy
// ---------------------------------------------------------------------------

TEST(RoundRobinExtTest, EmptyQueueReturnsInvalidPid)
{
    SimulationClock clock;
    RoundRobinPolicy policy(4);
    EXPECT_EQ(policy.selectNext({}, clock), INVALID_PID);
}

TEST(RoundRobinExtTest, ZeroTimeSliceNormalisedToOne)
{
    RoundRobinPolicy policy(0);
    EXPECT_EQ(policy.timeSlice(), 1u);
}

TEST(RoundRobinExtTest, SingleProcessSelected)
{
    SimulationClock clock;
    RoundRobinPolicy policy(4);
    std::vector<SchedulingProcessSnapshot> ready = {{.pid = 7, .lastStateChange = 10}};
    EXPECT_EQ(policy.selectNext(ready, clock), 7u);
}

TEST(RoundRobinExtTest, TieBreakByPidWhenLastStateChangeEqual)
{
    SimulationClock clock;
    RoundRobinPolicy policy(4);
    std::vector<SchedulingProcessSnapshot> ready = {
        {.pid = 3, .lastStateChange = 5},
        {.pid = 1, .lastStateChange = 5},
        {.pid = 2, .lastStateChange = 5},
    };
    EXPECT_EQ(policy.selectNext(ready, clock), 1u);
}

TEST(RoundRobinExtTest, SelectsEarliestLastStateChange)
{
    SimulationClock clock;
    RoundRobinPolicy policy(4);
    std::vector<SchedulingProcessSnapshot> ready = {
        {.pid = 1, .lastStateChange = 10},
        {.pid = 2, .lastStateChange = 3},
        {.pid = 3, .lastStateChange = 7},
    };
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(RoundRobinExtTest, NoPreemptionBeforeTimeSliceExpires)
{
    SimulationClock clock;
    RoundRobinPolicy policy(5);
    SchedulingProcessSnapshot running{.pid = 1, .lastStateChange = 0};
    SchedulingProcessSnapshot candidate{.pid = 2};
    clock.tick(); clock.tick(); // elapsed = 2, slice = 5
    EXPECT_FALSE(policy.shouldPreempt(running, candidate, clock));
}

TEST(RoundRobinExtTest, PreemptsExactlyAtTimeSliceBoundary)
{
    SimulationClock clock;
    RoundRobinPolicy policy(3);
    SchedulingProcessSnapshot running{.pid = 1, .lastStateChange = 0};
    SchedulingProcessSnapshot candidate{.pid = 2};
    clock.tick(); clock.tick(); clock.tick(); // elapsed = 3 >= slice = 3
    EXPECT_TRUE(policy.shouldPreempt(running, candidate, clock));
}

TEST(RoundRobinExtTest, PreemptsAfterTimeSliceExpires)
{
    SimulationClock clock;
    RoundRobinPolicy policy(2);
    SchedulingProcessSnapshot running{.pid = 1, .lastStateChange = 0};
    SchedulingProcessSnapshot candidate{.pid = 2};
    for (int i = 0; i < 5; ++i) clock.tick(); // elapsed = 5 > slice = 2
    EXPECT_TRUE(policy.shouldPreempt(running, candidate, clock));
}

TEST(RoundRobinExtTest, PolicyName)
{
    RoundRobinPolicy policy(2);
    EXPECT_EQ(policy.name(), "RoundRobin");
}

// ---------------------------------------------------------------------------
// FcfsPolicy
// ---------------------------------------------------------------------------

TEST(FcfsExtTest, EmptyQueueReturnsInvalidPid)
{
    SimulationClock clock;
    FcfsPolicy policy;
    EXPECT_EQ(policy.selectNext({}, clock), INVALID_PID);
}

TEST(FcfsExtTest, SingleProcessSelected)
{
    SimulationClock clock;
    FcfsPolicy policy;
    std::vector<SchedulingProcessSnapshot> ready = {{.pid = 5, .arrivalTime = 100}};
    EXPECT_EQ(policy.selectNext(ready, clock), 5u);
}

TEST(FcfsExtTest, TieBreakByPidWhenArrivalTimeEqual)
{
    SimulationClock clock;
    FcfsPolicy policy;
    std::vector<SchedulingProcessSnapshot> ready = {
        {.pid = 5, .arrivalTime = 10},
        {.pid = 2, .arrivalTime = 10},
        {.pid = 8, .arrivalTime = 10},
    };
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(FcfsExtTest, ThreeProcessesSelectEarliestArrival)
{
    SimulationClock clock;
    FcfsPolicy policy;
    std::vector<SchedulingProcessSnapshot> ready = {
        {.pid = 1, .arrivalTime = 15},
        {.pid = 2, .arrivalTime = 3},
        {.pid = 3, .arrivalTime = 9},
    };
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(FcfsExtTest, NeverPreemptsRunningProcess)
{
    SimulationClock clock;
    FcfsPolicy policy;
    SchedulingProcessSnapshot running{.pid = 1, .arrivalTime = 0};
    SchedulingProcessSnapshot candidate{.pid = 2, .arrivalTime = 0};
    EXPECT_FALSE(policy.shouldPreempt(running, candidate, clock));
}

TEST(FcfsExtTest, NeverPreemptsEvenWhenCandidateArrivedFirst)
{
    SimulationClock clock;
    FcfsPolicy policy;
    // candidate arrived before running — FCFS still does not preempt
    SchedulingProcessSnapshot running{.pid = 2, .arrivalTime = 10};
    SchedulingProcessSnapshot candidate{.pid = 1, .arrivalTime = 1};
    EXPECT_FALSE(policy.shouldPreempt(running, candidate, clock));
}

TEST(FcfsExtTest, PolicyName)
{
    FcfsPolicy policy;
    EXPECT_EQ(policy.name(), "FCFS");
}

// ---------------------------------------------------------------------------
// PriorityPolicy
// ---------------------------------------------------------------------------

TEST(PriorityExtTest, EmptyQueueReturnsInvalidPid)
{
    SimulationClock clock;
    PriorityPolicy policy;
    EXPECT_EQ(policy.selectNext({}, clock), INVALID_PID);
}

TEST(PriorityExtTest, SelectsRealtimeOverAllOthers)
{
    SimulationClock clock;
    PriorityPolicy policy;
    std::vector<SchedulingProcessSnapshot> ready = {
        {.pid = 1, .effectivePriority = PriorityLevel::Normal},
        {.pid = 2, .effectivePriority = PriorityLevel::Realtime},
        {.pid = 3, .effectivePriority = PriorityLevel::Low},
    };
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(PriorityExtTest, SelectsHighOverNormalAndLow)
{
    SimulationClock clock;
    PriorityPolicy policy;
    std::vector<SchedulingProcessSnapshot> ready = {
        {.pid = 1, .effectivePriority = PriorityLevel::Low},
        {.pid = 2, .effectivePriority = PriorityLevel::High},
        {.pid = 3, .effectivePriority = PriorityLevel::Normal},
    };
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(PriorityExtTest, PreemptsWhenCandidateIsHigherPriority)
{
    SimulationClock clock;
    PriorityPolicy policy;
    SchedulingProcessSnapshot running{.pid = 1, .effectivePriority = PriorityLevel::Normal};
    SchedulingProcessSnapshot candidate{.pid = 2, .effectivePriority = PriorityLevel::High};
    EXPECT_TRUE(policy.shouldPreempt(running, candidate, clock));
}

TEST(PriorityExtTest, NoPreemptWhenSamePriority)
{
    SimulationClock clock;
    PriorityPolicy policy;
    SchedulingProcessSnapshot running{.pid = 1, .effectivePriority = PriorityLevel::High};
    SchedulingProcessSnapshot candidate{.pid = 2, .effectivePriority = PriorityLevel::High};
    EXPECT_FALSE(policy.shouldPreempt(running, candidate, clock));
}

TEST(PriorityExtTest, NoPreemptWhenRunningHasHigherPriority)
{
    SimulationClock clock;
    PriorityPolicy policy;
    SchedulingProcessSnapshot running{.pid = 1, .effectivePriority = PriorityLevel::Realtime};
    SchedulingProcessSnapshot candidate{.pid = 2, .effectivePriority = PriorityLevel::High};
    EXPECT_FALSE(policy.shouldPreempt(running, candidate, clock));
}

TEST(PriorityExtTest, PolicyName)
{
    PriorityPolicy policy;
    EXPECT_EQ(policy.name(), "Priority");
}

// ---------------------------------------------------------------------------
// SpnPolicy
// ---------------------------------------------------------------------------

TEST(SpnExtTest, EmptyQueueReturnsInvalidPid)
{
    SimulationClock clock;
    SpnPolicy policy;
    EXPECT_EQ(policy.selectNext({}, clock), INVALID_PID);
}

TEST(SpnExtTest, SingleProcessSelected)
{
    SimulationClock clock;
    SpnPolicy policy;
    std::vector<SchedulingProcessSnapshot> ready = {{.pid = 9, .estimatedBurst = 42}};
    EXPECT_EQ(policy.selectNext(ready, clock), 9u);
}

TEST(SpnExtTest, SelectsSmallestBurstFromFour)
{
    SimulationClock clock;
    SpnPolicy policy;
    std::vector<SchedulingProcessSnapshot> ready = {
        {.pid = 1, .estimatedBurst = 100},
        {.pid = 2, .estimatedBurst = 5},
        {.pid = 3, .estimatedBurst = 50},
        {.pid = 4, .estimatedBurst = 8},
    };
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(SpnExtTest, NeverPreemptsEvenWhenCandidateHasSmallerBurst)
{
    SimulationClock clock;
    SpnPolicy policy;
    SchedulingProcessSnapshot running{.pid = 1, .estimatedBurst = 20};
    SchedulingProcessSnapshot candidate{.pid = 2, .estimatedBurst = 1};
    EXPECT_FALSE(policy.shouldPreempt(running, candidate, clock));
}

TEST(SpnExtTest, PolicyName)
{
    SpnPolicy policy;
    EXPECT_EQ(policy.name(), "SPN");
}

// ---------------------------------------------------------------------------
// SrtPolicy
// ---------------------------------------------------------------------------

TEST(SrtExtTest, EmptyQueueReturnsInvalidPid)
{
    SimulationClock clock;
    SrtPolicy policy;
    EXPECT_EQ(policy.selectNext({}, clock), INVALID_PID);
}

TEST(SrtExtTest, SelectsSmallestRemainingBurst)
{
    SimulationClock clock;
    SrtPolicy policy;
    std::vector<SchedulingProcessSnapshot> ready = {
        {.pid = 1, .remainingBurst = 10},
        {.pid = 2, .remainingBurst = 3},
        {.pid = 3, .remainingBurst = 7},
    };
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(SrtExtTest, PreemptsWhenCandidateHasSmallerRemaining)
{
    SimulationClock clock;
    SrtPolicy policy;
    SchedulingProcessSnapshot running{.pid = 1, .remainingBurst = 8};
    SchedulingProcessSnapshot candidate{.pid = 2, .remainingBurst = 3};
    EXPECT_TRUE(policy.shouldPreempt(running, candidate, clock));
}

TEST(SrtExtTest, NoPreemptWhenEqualRemainingBurst)
{
    SimulationClock clock;
    SrtPolicy policy;
    SchedulingProcessSnapshot running{.pid = 1, .remainingBurst = 5};
    SchedulingProcessSnapshot candidate{.pid = 2, .remainingBurst = 5};
    EXPECT_FALSE(policy.shouldPreempt(running, candidate, clock));
}

TEST(SrtExtTest, NoPreemptWhenRunningHasSmallerRemaining)
{
    SimulationClock clock;
    SrtPolicy policy;
    SchedulingProcessSnapshot running{.pid = 1, .remainingBurst = 2};
    SchedulingProcessSnapshot candidate{.pid = 2, .remainingBurst = 9};
    EXPECT_FALSE(policy.shouldPreempt(running, candidate, clock));
}

TEST(SrtExtTest, PolicyName)
{
    SrtPolicy policy;
    EXPECT_EQ(policy.name(), "SRT");
}

// ---------------------------------------------------------------------------
// HrrnPolicy
// ---------------------------------------------------------------------------

TEST(HrrnExtTest, EmptyQueueReturnsInvalidPid)
{
    SimulationClock clock;
    HrrnPolicy policy;
    EXPECT_EQ(policy.selectNext({}, clock), INVALID_PID);
}

TEST(HrrnExtTest, SelectsHighestResponseRatioFromThree)
{
    SimulationClock clock;
    HrrnPolicy policy;
    // Ratios: p1=(10+5)/10=1.5, p2=(4+12)/4=4.0, p3=(5+5)/5=2.0 → p2 wins
    std::vector<SchedulingProcessSnapshot> ready = {
        {.pid = 1, .estimatedBurst = 10, .totalWaitTime = 5},
        {.pid = 2, .estimatedBurst = 4, .totalWaitTime = 12},
        {.pid = 3, .estimatedBurst = 5, .totalWaitTime = 5},
    };
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(HrrnExtTest, LongerWaitingWinsWhenBurstsEqual)
{
    SimulationClock clock;
    HrrnPolicy policy;
    // p1=(10+2)/10=1.2, p2=(10+20)/10=3.0 → p2 wins
    std::vector<SchedulingProcessSnapshot> ready = {
        {.pid = 1, .estimatedBurst = 10, .totalWaitTime = 2},
        {.pid = 2, .estimatedBurst = 10, .totalWaitTime = 20},
    };
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(HrrnExtTest, NeverPreempts)
{
    SimulationClock clock;
    HrrnPolicy policy;
    SchedulingProcessSnapshot running{.pid = 1, .estimatedBurst = 10, .totalWaitTime = 0};
    SchedulingProcessSnapshot candidate{.pid = 2, .estimatedBurst = 1, .totalWaitTime = 100};
    EXPECT_FALSE(policy.shouldPreempt(running, candidate, clock));
}

TEST(HrrnExtTest, PolicyName)
{
    HrrnPolicy policy;
    EXPECT_EQ(policy.name(), "HRRN");
}

// ---------------------------------------------------------------------------
// MlfqPolicy
// ---------------------------------------------------------------------------

TEST(MlfqExtTest, EmptyQueueReturnsInvalidPid)
{
    SimulationClock clock;
    MlfqPolicy policy;
    EXPECT_EQ(policy.selectNext({}, clock), INVALID_PID);
}

TEST(MlfqExtTest, ZeroTimeSlicesNormalisedToOne)
{
    MlfqPolicy policy({0, 0, 0});
    for (std::size_t slice : policy.levelTimeSlices())
        EXPECT_EQ(slice, 1u);
}

TEST(MlfqExtTest, EmptyTimeSliceListDefaultsToOneLevelOne)
{
    MlfqPolicy policy({});
    ASSERT_EQ(policy.levelTimeSlices().size(), 1u);
    EXPECT_EQ(policy.levelTimeSlices().front(), 1u);
}

TEST(MlfqExtTest, HigherPriorityLevelWinsOverLower)
{
    SimulationClock clock;
    MlfqPolicy policy({1, 2, 4});
    std::vector<SchedulingProcessSnapshot> ready = {
        {.pid = 1, .lastStateChange = 0, .effectivePriority = PriorityLevel::Low},
        {.pid = 2, .lastStateChange = 0, .effectivePriority = PriorityLevel::Realtime},
        {.pid = 3, .lastStateChange = 0, .effectivePriority = PriorityLevel::Normal},
    };
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(MlfqExtTest, SameLevelTieBreakByLastStateChange)
{
    SimulationClock clock;
    MlfqPolicy policy({2, 4, 8});
    // All High priority — earliest lastStateChange wins
    std::vector<SchedulingProcessSnapshot> ready = {
        {.pid = 3, .lastStateChange = 10, .effectivePriority = PriorityLevel::High},
        {.pid = 1, .lastStateChange = 5, .effectivePriority = PriorityLevel::High},
        {.pid = 2, .lastStateChange = 8, .effectivePriority = PriorityLevel::High},
    };
    EXPECT_EQ(policy.selectNext(ready, clock), 1u);
}

TEST(MlfqExtTest, TimeSliceExpiryPreemptsWithinSameLevel)
{
    SimulationClock clock;
    // Realtime → level 0, slice = 2
    MlfqPolicy policy({2, 4, 8});
    SchedulingProcessSnapshot running{
        .pid = 1, .lastStateChange = 0, .effectivePriority = PriorityLevel::Realtime
    };
    SchedulingProcessSnapshot candidate{
        .pid = 2, .lastStateChange = 0, .effectivePriority = PriorityLevel::Realtime
    };
    clock.tick(); // elapsed = 1 < 2 → no preempt
    EXPECT_FALSE(policy.shouldPreempt(running, candidate, clock));
    clock.tick(); // elapsed = 2 >= 2 → preempt
    EXPECT_TRUE(policy.shouldPreempt(running, candidate, clock));
}

TEST(MlfqExtTest, HigherPriorityCandidatePreemptsImmediately)
{
    SimulationClock clock;
    MlfqPolicy policy({2, 4, 8});
    // Running is Normal (level >= 3, clamped to 2, slice = 8)
    // Candidate is Realtime (level 0) — preempt immediately regardless of time
    SchedulingProcessSnapshot running{
        .pid = 1, .lastStateChange = 0, .effectivePriority = PriorityLevel::Normal
    };
    SchedulingProcessSnapshot candidate{
        .pid = 2, .lastStateChange = 0, .effectivePriority = PriorityLevel::Realtime
    };
    clock.tick(); // only 1 tick elapsed — well within any slice
    EXPECT_TRUE(policy.shouldPreempt(running, candidate, clock));
}

TEST(MlfqExtTest, PolicyName)
{
    MlfqPolicy policy;
    EXPECT_EQ(policy.name(), "MLFQ");
}
