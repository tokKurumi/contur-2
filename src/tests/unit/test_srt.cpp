/// @file test_srt.cpp
/// @brief Unit tests for SRT scheduling policy.

#include <functional>

#include <gtest/gtest.h>

#include "contur/core/clock.h"

#include "contur/process/pcb.h"
#include "contur/scheduling/srt_policy.h"

using namespace contur;

TEST(SrtPolicyTest, SelectsSmallestRemainingBurst)
{
    SimulationClock clock;
    SrtPolicy policy;

    PCB p1(1, "p1");
    PCB p2(2, "p2");
    PCB p3(3, "p3");

    p1.timing().remainingBurst = 8;
    p2.timing().remainingBurst = 2;
    p3.timing().remainingBurst = 5;

    std::vector<std::reference_wrapper<const PCB>> ready = {std::cref(p1), std::cref(p2), std::cref(p3)};
    EXPECT_EQ(policy.selectNext(ready, clock), 2u);
}

TEST(SrtPolicyTest, PreemptsWhenCandidateHasSmallerRemaining)
{
    SimulationClock clock;
    SrtPolicy policy;

    PCB running(1, "running");
    PCB candidate(2, "candidate");

    running.timing().remainingBurst = 6;
    candidate.timing().remainingBurst = 2;

    EXPECT_TRUE(policy.shouldPreempt(running, candidate, clock));
}
