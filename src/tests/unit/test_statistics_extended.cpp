/// @file test_statistics_extended.cpp
/// @brief Extended unit tests for Statistics — multiple processes, reset,
///        EWMA convergence, alpha boundary, predictedBurst with no history.

#include <cmath>

#include <gtest/gtest.h>

#include "contur/scheduling/statistics.h"

using namespace contur;

// Multiple independent processes
TEST(StatisticsExtTest, MultipleProcessesTrackIndependently)
{
    Statistics stats;

    stats.recordBurst(1, 10);
    stats.recordBurst(2, 20);
    stats.recordBurst(3, 30);

    EXPECT_EQ(stats.predictedBurst(1), 10u);
    EXPECT_EQ(stats.predictedBurst(2), 20u);
    EXPECT_EQ(stats.predictedBurst(3), 30u);
}

TEST(StatisticsExtTest, HasPredictionIsFalseForUntrackedProcess)
{
    Statistics stats;
    stats.recordBurst(1, 5);

    EXPECT_FALSE(stats.hasPrediction(999));
    EXPECT_EQ(stats.predictedBurst(999), 0u);
}

// reset() clears all processes
TEST(StatisticsExtTest, ResetClearsAllProcesses)
{
    Statistics stats;
    stats.recordBurst(1, 10);
    stats.recordBurst(2, 20);
    stats.recordBurst(3, 30);

    stats.reset();

    EXPECT_FALSE(stats.hasPrediction(1));
    EXPECT_FALSE(stats.hasPrediction(2));
    EXPECT_FALSE(stats.hasPrediction(3));
}

TEST(StatisticsExtTest, RecordAfterResetStartsFresh)
{
    Statistics stats(0.5);
    stats.recordBurst(1, 100);
    stats.recordBurst(1, 50); // prediction: (100+50)/2 = 75
    ASSERT_EQ(stats.predictedBurst(1), 75u);

    stats.reset();
    stats.recordBurst(1, 10); // first record again → prediction = 10
    EXPECT_EQ(stats.predictedBurst(1), 10u);
}

// clear() vs reset()
TEST(StatisticsExtTest, ClearOneProcessDoesNotAffectOthers)
{
    Statistics stats;
    stats.recordBurst(1, 10);
    stats.recordBurst(2, 20);

    stats.clear(1);

    EXPECT_FALSE(stats.hasPrediction(1));
    EXPECT_TRUE(stats.hasPrediction(2));
    EXPECT_EQ(stats.predictedBurst(2), 20u);
}

// EWMA convergence
TEST(StatisticsExtTest, EwmaWithAlphaOneAlwaysReturnsLatestBurst)
{
    // alpha=1.0 → prediction = new burst every time
    Statistics stats(1.0);
    stats.recordBurst(1, 100);
    EXPECT_EQ(stats.predictedBurst(1), 100u);

    stats.recordBurst(1, 50);
    EXPECT_EQ(stats.predictedBurst(1), 50u);

    stats.recordBurst(1, 7);
    EXPECT_EQ(stats.predictedBurst(1), 7u);
}

TEST(StatisticsExtTest, EwmaWithAlphaHalfAveragesCorrectly)
{
    // alpha=0.5 → new = 0.5*burst + 0.5*old
    // 1st burst = 10 → prediction = 10
    // 2nd burst = 6  → 0.5*6 + 0.5*10 = 8
    // 3rd burst = 4  → 0.5*4 + 0.5*8  = 6
    Statistics stats(0.5);
    stats.recordBurst(1, 10);
    EXPECT_EQ(stats.predictedBurst(1), 10u);

    stats.recordBurst(1, 6);
    EXPECT_EQ(stats.predictedBurst(1), 8u);

    stats.recordBurst(1, 4);
    EXPECT_EQ(stats.predictedBurst(1), 6u);
}

TEST(StatisticsExtTest, EwmaConvergesAfterManyBurstsOfSameValue)
{
    // After many identical bursts, prediction should stabilise at that value.
    Statistics stats(0.5);
    stats.recordBurst(1, 0);
    for (int i = 0; i < 30; ++i)
    {
        stats.recordBurst(1, 100);
    }

    // After 30 identical 100-bursts starting from 0, prediction ≈ 100
    EXPECT_GE(stats.predictedBurst(1), 99u);
    EXPECT_LE(stats.predictedBurst(1), 100u);
}

// Alpha query
TEST(StatisticsExtTest, AlphaQueryMatchesConstructorArgument)
{
    Statistics s025(0.25);
    Statistics s075(0.75);

    EXPECT_DOUBLE_EQ(s025.alpha(), 0.25);
    EXPECT_DOUBLE_EQ(s075.alpha(), 0.75);
}

TEST(StatisticsExtTest, DefaultAlphaIsHalf)
{
    Statistics stats;
    EXPECT_DOUBLE_EQ(stats.alpha(), 0.5);
}

// Move semantics
TEST(StatisticsExtTest, MoveConstructedStatsOwnsData)
{
    Statistics src(0.5);
    src.recordBurst(1, 20);

    Statistics dst(std::move(src));

    EXPECT_TRUE(dst.hasPrediction(1));
    EXPECT_EQ(dst.predictedBurst(1), 20u);
    EXPECT_DOUBLE_EQ(dst.alpha(), 0.5);
}
