/// @file test_clock.cpp
/// @brief Unit tests for SimulationClock.

#include <gtest/gtest.h>

#include "contur/core/clock.h"

using namespace contur;

TEST(SimulationClockTest, StartsAtZero)
{
    SimulationClock clock;
    EXPECT_EQ(clock.now(), 0u);
}

TEST(SimulationClockTest, TickIncrementsTime)
{
    SimulationClock clock;
    clock.tick();
    EXPECT_EQ(clock.now(), 1u);
    clock.tick();
    EXPECT_EQ(clock.now(), 2u);
    clock.tick();
    EXPECT_EQ(clock.now(), 3u);
}

TEST(SimulationClockTest, ResetSetsToZero)
{
    SimulationClock clock;
    clock.tick();
    clock.tick();
    clock.tick();
    EXPECT_EQ(clock.now(), 3u);

    clock.reset();
    EXPECT_EQ(clock.now(), 0u);
}

TEST(SimulationClockTest, ResetThenTickContinuesFromZero)
{
    SimulationClock clock;
    for (int i = 0; i < 10; ++i)
    {
        clock.tick();
    }
    EXPECT_EQ(clock.now(), 10u);

    clock.reset();
    clock.tick();
    EXPECT_EQ(clock.now(), 1u);
}

TEST(SimulationClockTest, MoveConstruction)
{
    SimulationClock clock;
    clock.tick();
    clock.tick();

    SimulationClock moved(std::move(clock));
    EXPECT_EQ(moved.now(), 2u);
}

TEST(SimulationClockTest, MoveAssignment)
{
    SimulationClock clock;
    clock.tick();
    clock.tick();
    clock.tick();

    SimulationClock other;
    other = std::move(clock);
    EXPECT_EQ(other.now(), 3u);
}

TEST(SimulationClockTest, InterfacePolymorphism)
{
    auto clock = std::make_unique<SimulationClock>();
    IClock *iface = clock.get();

    EXPECT_EQ(iface->now(), 0u);
    iface->tick();
    EXPECT_EQ(iface->now(), 1u);
    iface->reset();
    EXPECT_EQ(iface->now(), 0u);
}

TEST(SimulationClockTest, ManyTicks)
{
    SimulationClock clock;
    constexpr Tick N = 10000;
    for (Tick i = 0; i < N; ++i)
    {
        clock.tick();
    }
    EXPECT_EQ(clock.now(), N);
}
