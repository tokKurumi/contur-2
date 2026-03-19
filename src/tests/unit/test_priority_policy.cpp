/// @file test_priority_policy.cpp
/// @brief Unit tests for Priority scheduling policy.

#include <functional>

#include <gtest/gtest.h>

#include "contur/core/clock.h"

#include "contur/process/pcb.h"
#include "contur/scheduling/priority_policy.h"

using namespace contur;

TEST(PriorityPolicyTest, SelectsHighestEffectivePriority)
{
    SimulationClock clock;
    PriorityPolicy policy;

    PCB low(1, "low", Priority(PriorityLevel::Low));
    PCB high(2, "high", Priority(PriorityLevel::High));
    PCB normal(3, "normal", Priority(PriorityLevel::Normal));

    std::vector<std::reference_wrapper<const PCB>> ready = {std::cref(low), std::cref(high), std::cref(normal)};
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(PriorityPolicyTest, PreemptsWhenCandidateHigherPriority)
{
    SimulationClock clock;
    PriorityPolicy policy;

    PCB running(1, "running", Priority(PriorityLevel::Normal));
    PCB candidate(2, "candidate", Priority(PriorityLevel::High));

    EXPECT_TRUE(policy.shouldPreempt(running, candidate, clock));
}
