/// @file test_threading_config.cpp
/// @brief Unit tests for HostThreadingConfig.

#include <gtest/gtest.h>

#include "contur/dispatch/threading_config.h"

using namespace contur;

TEST(ThreadingConfigTest, DefaultsAreValidAndSingleThreaded)
{
    HostThreadingConfig config;

    EXPECT_TRUE(config.isValid());
    EXPECT_TRUE(config.isSingleThreaded());
    EXPECT_TRUE(config.deterministicMode);
    EXPECT_FALSE(config.workStealingEnabled);
}

TEST(ThreadingConfigTest, ZeroThreadCountIsInvalidBeforeNormalization)
{
    HostThreadingConfig config;
    config.hostThreadCount = 0;
    config.workStealingEnabled = true;

    EXPECT_FALSE(config.isValid());
    EXPECT_FALSE(config.isSingleThreaded());
}

TEST(ThreadingConfigTest, NormalizedConvertsZeroThreadCountToSingleThread)
{
    HostThreadingConfig config;
    config.hostThreadCount = 0;
    config.workStealingEnabled = true;

    const HostThreadingConfig normalized = config.normalized();

    EXPECT_EQ(normalized.hostThreadCount, 1U);
    EXPECT_TRUE(normalized.isValid());
    EXPECT_TRUE(normalized.isSingleThreaded());
    EXPECT_FALSE(normalized.workStealingEnabled);
    EXPECT_TRUE(normalized.deterministicMode);
}

TEST(ThreadingConfigTest, NormalizeMutatesInPlace)
{
    HostThreadingConfig config;
    config.hostThreadCount = 0;
    config.workStealingEnabled = true;

    config.normalize();

    EXPECT_EQ(config.hostThreadCount, 1U);
    EXPECT_TRUE(config.isValid());
    EXPECT_TRUE(config.isSingleThreaded());
    EXPECT_FALSE(config.workStealingEnabled);
}

TEST(ThreadingConfigTest, MultiThreadedNormalizedConfigKeepsWorkStealingFlag)
{
    HostThreadingConfig config;
    config.hostThreadCount = 4;
    config.workStealingEnabled = true;
    config.deterministicMode = false;

    const HostThreadingConfig normalized = config.normalized();

    EXPECT_EQ(normalized.hostThreadCount, 4U);
    EXPECT_TRUE(normalized.isValid());
    EXPECT_FALSE(normalized.isSingleThreaded());
    EXPECT_TRUE(normalized.workStealingEnabled);
    EXPECT_FALSE(normalized.deterministicMode);
}