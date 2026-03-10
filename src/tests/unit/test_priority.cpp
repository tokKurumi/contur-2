/// @file test_priority.cpp
/// @brief Unit tests for PriorityLevel enum and Priority struct.

#include <gtest/gtest.h>

#include "contur/process/priority.h"

using namespace contur;

// PriorityLevel ordering

TEST(PriorityLevelTest, RealtimeIsHighest)
{
    EXPECT_LT(static_cast<int>(PriorityLevel::Realtime), static_cast<int>(PriorityLevel::High));
}

TEST(PriorityLevelTest, IdleIsLowest)
{
    EXPECT_GT(static_cast<int>(PriorityLevel::Idle), static_cast<int>(PriorityLevel::Low));
}

TEST(PriorityLevelTest, NaturalOrdering)
{
    EXPECT_LT(static_cast<int>(PriorityLevel::Realtime), static_cast<int>(PriorityLevel::High));
    EXPECT_LT(static_cast<int>(PriorityLevel::High), static_cast<int>(PriorityLevel::AboveNormal));
    EXPECT_LT(static_cast<int>(PriorityLevel::AboveNormal), static_cast<int>(PriorityLevel::Normal));
    EXPECT_LT(static_cast<int>(PriorityLevel::Normal), static_cast<int>(PriorityLevel::BelowNormal));
    EXPECT_LT(static_cast<int>(PriorityLevel::BelowNormal), static_cast<int>(PriorityLevel::Low));
    EXPECT_LT(static_cast<int>(PriorityLevel::Low), static_cast<int>(PriorityLevel::Idle));
}

// priorityLevelName

TEST(PriorityLevelTest, NameReturnsCorrectStrings)
{
    EXPECT_EQ(priorityLevelName(PriorityLevel::Realtime), "Realtime");
    EXPECT_EQ(priorityLevelName(PriorityLevel::High), "High");
    EXPECT_EQ(priorityLevelName(PriorityLevel::AboveNormal), "AboveNormal");
    EXPECT_EQ(priorityLevelName(PriorityLevel::Normal), "Normal");
    EXPECT_EQ(priorityLevelName(PriorityLevel::BelowNormal), "BelowNormal");
    EXPECT_EQ(priorityLevelName(PriorityLevel::Low), "Low");
    EXPECT_EQ(priorityLevelName(PriorityLevel::Idle), "Idle");
}

// Priority construction

TEST(PriorityTest, DefaultConstructionIsNormal)
{
    constexpr Priority p;
    EXPECT_EQ(p.base, PriorityLevel::Normal);
    EXPECT_EQ(p.effective, PriorityLevel::Normal);
    EXPECT_EQ(p.nice, NICE_DEFAULT);
}

TEST(PriorityTest, SingleLevelConstruction)
{
    constexpr Priority p(PriorityLevel::High);
    EXPECT_EQ(p.base, PriorityLevel::High);
    EXPECT_EQ(p.effective, PriorityLevel::High);
    EXPECT_EQ(p.nice, NICE_DEFAULT);
}

TEST(PriorityTest, FullConstruction)
{
    constexpr Priority p(PriorityLevel::High, PriorityLevel::Realtime, -10);
    EXPECT_EQ(p.base, PriorityLevel::High);
    EXPECT_EQ(p.effective, PriorityLevel::Realtime);
    EXPECT_EQ(p.nice, -10);
}

// Nice clamping

TEST(PriorityTest, NiceClampLow)
{
    constexpr Priority p(PriorityLevel::Normal, PriorityLevel::Normal, -100);
    EXPECT_EQ(p.nice, NICE_MIN);
}

TEST(PriorityTest, NiceClampHigh)
{
    constexpr Priority p(PriorityLevel::Normal, PriorityLevel::Normal, 100);
    EXPECT_EQ(p.nice, NICE_MAX);
}

TEST(PriorityTest, NiceClampMinBoundary)
{
    EXPECT_EQ(Priority::clampNice(-20), -20);
    EXPECT_EQ(Priority::clampNice(-21), -20);
}

TEST(PriorityTest, NiceClampMaxBoundary)
{
    EXPECT_EQ(Priority::clampNice(19), 19);
    EXPECT_EQ(Priority::clampNice(20), 19);
}

TEST(PriorityTest, NiceClampInRange)
{
    EXPECT_EQ(Priority::clampNice(0), 0);
    EXPECT_EQ(Priority::clampNice(5), 5);
    EXPECT_EQ(Priority::clampNice(-5), -5);
}

// Equality / inequality

TEST(PriorityTest, EqualityDefaultConstructed)
{
    constexpr Priority a;
    constexpr Priority b;
    EXPECT_EQ(a, b);
}

TEST(PriorityTest, EqualitySameValues)
{
    constexpr Priority a(PriorityLevel::High, PriorityLevel::Realtime, -10);
    constexpr Priority b(PriorityLevel::High, PriorityLevel::Realtime, -10);
    EXPECT_EQ(a, b);
}

TEST(PriorityTest, InequalityDifferentBase)
{
    constexpr Priority a(PriorityLevel::High);
    constexpr Priority b(PriorityLevel::Low);
    EXPECT_NE(a, b);
}

TEST(PriorityTest, InequalityDifferentEffective)
{
    Priority a(PriorityLevel::Normal);
    Priority b(PriorityLevel::Normal);
    b.effective = PriorityLevel::High;
    EXPECT_NE(a, b);
}

TEST(PriorityTest, InequalityDifferentNice)
{
    Priority a(PriorityLevel::Normal, PriorityLevel::Normal, 0);
    Priority b(PriorityLevel::Normal, PriorityLevel::Normal, 5);
    EXPECT_NE(a, b);
}

// isHigherThan comparison

TEST(PriorityTest, HigherEffectiveLevelWins)
{
    constexpr Priority high(PriorityLevel::High);
    constexpr Priority normal(PriorityLevel::Normal);
    EXPECT_TRUE(high.isHigherThan(normal));
    EXPECT_FALSE(normal.isHigherThan(high));
}

TEST(PriorityTest, SameEffectiveLevelNiceBreaksTie)
{
    Priority a(PriorityLevel::Normal, PriorityLevel::Normal, -5);
    Priority b(PriorityLevel::Normal, PriorityLevel::Normal, 5);
    EXPECT_TRUE(a.isHigherThan(b)); // Lower nice = higher priority
    EXPECT_FALSE(b.isHigherThan(a));
}

TEST(PriorityTest, SameEffectiveSameNiceNotHigher)
{
    constexpr Priority a(PriorityLevel::Normal);
    constexpr Priority b(PriorityLevel::Normal);
    EXPECT_FALSE(a.isHigherThan(b));
    EXPECT_FALSE(b.isHigherThan(a));
}

TEST(PriorityTest, RealtimeHigherThanIdle)
{
    constexpr Priority rt(PriorityLevel::Realtime);
    constexpr Priority idle(PriorityLevel::Idle);
    EXPECT_TRUE(rt.isHigherThan(idle));
    EXPECT_FALSE(idle.isHigherThan(rt));
}

// Constexpr verification

TEST(PriorityTest, ConstexprConstruction)
{
    static_assert(Priority{}.base == PriorityLevel::Normal);
    static_assert(Priority(PriorityLevel::High).effective == PriorityLevel::High);
    static_assert(Priority::clampNice(-100) == NICE_MIN);
    static_assert(Priority::clampNice(100) == NICE_MAX);
    static_assert(Priority(PriorityLevel::High).isHigherThan(Priority(PriorityLevel::Low)));
}
