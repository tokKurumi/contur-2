/// @file test_mlfq.cpp
/// @brief Unit tests for MLFQ scheduling policy.

#include <functional>

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

    std::vector<std::reference_wrapper<const PCB>> ready = {std::cref(low), std::cref(high)};
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

    EXPECT_TRUE(policy.shouldPreempt(running, candidate, clock));
}
