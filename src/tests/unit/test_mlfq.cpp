/// @file test_mlfq.cpp
/// @brief Unit tests for MLFQ scheduling policy.

#include <gtest/gtest.h>

#include "contur/core/clock.h"

#include "contur/process/pcb.h"
#include "contur/process/state.h"
#include "contur/scheduling/mlfq_policy.h"

using namespace contur;

TEST(MlfqPolicyTest, SelectsFromHighestPriorityLevel)
{
    SimulationClock clock;
    MlfqPolicy policy({1, 2, 4});

    PCB low(1, "low", Priority(PriorityLevel::Low));
    PCB high(2, "high", Priority(PriorityLevel::High));

    std::vector<SchedulingProcessSnapshot> ready = {
        {.pid = low.id(),
         .lastStateChange = low.timing().lastStateChange,
         .effectivePriority = low.priority().effective},
        {.pid = high.id(),
         .lastStateChange = high.timing().lastStateChange,
         .effectivePriority = high.priority().effective},
    };
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(MlfqPolicyTest, PreemptsForHigherPriorityCandidate)
{
    SimulationClock clock;
    MlfqPolicy policy({2, 4, 8});

    PCB running(1, "running", Priority(PriorityLevel::BelowNormal));
    PCB candidate(2, "candidate", Priority(PriorityLevel::High));

    ASSERT_TRUE(running.setState(ProcessState::Ready, 0));
    ASSERT_TRUE(running.setState(ProcessState::Running, 1));

    SchedulingProcessSnapshot runningSnapshot{
        .pid = running.id(),
        .lastStateChange = running.timing().lastStateChange,
        .effectivePriority = running.priority().effective,
    };
    SchedulingProcessSnapshot candidateSnapshot{
        .pid = candidate.id(),
        .lastStateChange = candidate.timing().lastStateChange,
        .effectivePriority = candidate.priority().effective,
    };
    EXPECT_TRUE(policy.shouldPreempt(runningSnapshot, candidateSnapshot, clock));
}
