/// @file test_hrrn.cpp
/// @brief Unit tests for HRRN scheduling policy.

#include <functional>

#include <gtest/gtest.h>

#include "contur/core/clock.h"

#include "contur/process/pcb.h"
#include "contur/scheduling/hrrn_policy.h"

using namespace contur;

TEST(HrrnPolicyTest, SelectsHighestResponseRatio)
{
    SimulationClock clock;
    HrrnPolicy policy;

    PCB p1(1, "p1");
    PCB p2(2, "p2");
    PCB p3(3, "p3");

    p1.timing().estimatedBurst = 10;
    p1.timing().totalWaitTime = 5; // ratio 1.5

    p2.timing().estimatedBurst = 4;
    p2.timing().totalWaitTime = 12; // ratio 4.0

    p3.timing().estimatedBurst = 5;
    p3.timing().totalWaitTime = 5; // ratio 2.0

    std::vector<std::reference_wrapper<const PCB>> ready = {std::cref(p1), std::cref(p2), std::cref(p3)};
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(HrrnPolicyTest, ShouldNotPreempt)
{
    SimulationClock clock;
    HrrnPolicy policy;
    PCB running(1, "running");
    PCB candidate(2, "candidate");
    EXPECT_FALSE(policy.shouldPreempt(running, candidate, clock));
}
