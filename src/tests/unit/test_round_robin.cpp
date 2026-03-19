/// @file test_round_robin.cpp
/// @brief Unit tests for Round Robin scheduling policy.

#include <functional>

#include <gtest/gtest.h>

#include "contur/core/clock.h"

#include "contur/process/pcb.h"
#include "contur/process/state.h"
#include "contur/scheduling/round_robin_policy.h"

using namespace contur;

TEST(RoundRobinPolicyTest, SelectsOldestReadyByLastStateChange)
{
    SimulationClock clock;
    RoundRobinPolicy policy(3);

    PCB p1(1, "p1");
    PCB p2(2, "p2");

    ASSERT_TRUE(p1.setState(ProcessState::Ready, 3));
    ASSERT_TRUE(p2.setState(ProcessState::Ready, 1));

    std::vector<std::reference_wrapper<const PCB>> ready = {std::cref(p1), std::cref(p2)};
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(RoundRobinPolicyTest, PreemptsWhenTimeSliceExpires)
{
    SimulationClock clock;
    RoundRobinPolicy policy(2);

    PCB running(1, "running");
    PCB candidate(2, "candidate");

    ASSERT_TRUE(running.setState(ProcessState::Ready, 0));
    ASSERT_TRUE(running.setState(ProcessState::Running, 1));

    clock.tick(); // now = 1
    clock.tick(); // now = 2
    clock.tick(); // now = 3

    EXPECT_TRUE(policy.shouldPreempt(running, candidate, clock));
}

TEST(RoundRobinPolicyTest, DoesNotPreemptBeforeTimeSlice)
{
    SimulationClock clock;
    RoundRobinPolicy policy(5);

    PCB running(1, "running");
    PCB candidate(2, "candidate");

    ASSERT_TRUE(running.setState(ProcessState::Ready, 0));
    ASSERT_TRUE(running.setState(ProcessState::Running, 1));

    clock.tick(); // now = 1
    clock.tick(); // now = 2

    EXPECT_FALSE(policy.shouldPreempt(running, candidate, clock));
}
