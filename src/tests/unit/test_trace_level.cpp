/// @file test_trace_level.cpp
/// @brief Unit tests for TraceLevel string conversion and ordering invariants,
///        plus the makeTraceEvent factory helper.

#include <cstdint>

#include <gtest/gtest.h>

#include "contur/tracing/trace_event.h"
#include "contur/tracing/trace_level.h"

using namespace contur;

TEST(TraceLevelTest, ToStringForAllKnownLevels)
{
    EXPECT_EQ(traceLevelToString(TraceLevel::Debug), "debug");
    EXPECT_EQ(traceLevelToString(TraceLevel::Info), "info");
    EXPECT_EQ(traceLevelToString(TraceLevel::Warn), "warn");
    EXPECT_EQ(traceLevelToString(TraceLevel::Error), "error");
}

TEST(TraceLevelTest, ToStringForUnknownLevelReturnsUnknown)
{
    // Cast a number outside the enum range — switch falls through to "unknown".
    auto bogus = static_cast<TraceLevel>(static_cast<std::uint8_t>(99));
    EXPECT_EQ(traceLevelToString(bogus), "unknown");
}

TEST(TraceLevelTest, NumericOrderingMatchesSeverity)
{
    // Severity goes Debug < Info < Warn < Error.
    EXPECT_LT(static_cast<std::uint8_t>(TraceLevel::Debug), static_cast<std::uint8_t>(TraceLevel::Info));
    EXPECT_LT(static_cast<std::uint8_t>(TraceLevel::Info), static_cast<std::uint8_t>(TraceLevel::Warn));
    EXPECT_LT(static_cast<std::uint8_t>(TraceLevel::Warn), static_cast<std::uint8_t>(TraceLevel::Error));
}

TEST(MakeTraceEventTest, PopulatesAllExplicitFields)
{
    auto event = makeTraceEvent(7, "Subsystem", "operation", "details=ok", 3, TraceLevel::Warn);

    EXPECT_EQ(event.timestamp, 7u);
    EXPECT_EQ(event.subsystem, "Subsystem");
    EXPECT_EQ(event.operation, "operation");
    EXPECT_EQ(event.details, "details=ok");
    EXPECT_EQ(event.depth, 3u);
    EXPECT_EQ(event.level, TraceLevel::Warn);
}

TEST(MakeTraceEventTest, DefaultLevelIsInfo)
{
    auto event = makeTraceEvent(0, "S", "op", "", 0);
    EXPECT_EQ(event.level, TraceLevel::Info);
}

TEST(MakeTraceEventTest, LeavesUnspecifiedRuntimeFieldsAtDefaults)
{
    auto event = makeTraceEvent(1, "S", "op", "", 0);
    EXPECT_EQ(event.workerId, 0u);
    EXPECT_EQ(event.sequence, 0u);
    EXPECT_EQ(event.epoch, 0u);
}

TEST(MakeTraceEventTest, AcceptsEmptyStrings)
{
    auto event = makeTraceEvent(0, "", "", "", 0);
    EXPECT_TRUE(event.subsystem.empty());
    EXPECT_TRUE(event.operation.empty());
    EXPECT_TRUE(event.details.empty());
}
