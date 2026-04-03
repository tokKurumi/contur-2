/// @file test_tui_models.cpp
/// @brief Unit tests for TUI model DTO contracts.

#include <gtest/gtest.h>

#include "contur/tui/tui_models.h"

using namespace contur;

TEST(TuiModelsTest, ProcessSnapshotDefaultsAreSane)
{
    TuiProcessSnapshot process;

    EXPECT_EQ(process.id, INVALID_PID);
    EXPECT_TRUE(process.name.empty());
    EXPECT_EQ(process.state, ProcessState::New);
    EXPECT_EQ(process.basePriority, PriorityLevel::Normal);
    EXPECT_EQ(process.effectivePriority, PriorityLevel::Normal);
    EXPECT_EQ(process.nice, NICE_DEFAULT);
    EXPECT_EQ(process.cpuTime, 0u);
    EXPECT_FALSE(process.laneIndex.has_value());
}

TEST(TuiModelsTest, HistoryEntryStoresSnapshotCopy)
{
    TuiSnapshot snapshot;
    snapshot.currentTick = 11;
    snapshot.processCount = 2;
    snapshot.scheduler.runningQueue = {3, 7};

    TuiHistoryEntry entry;
    entry.sequence = 5;
    entry.snapshot = snapshot;

    snapshot.currentTick = 99;

    EXPECT_EQ(entry.sequence, 5u);
    EXPECT_EQ(entry.snapshot.currentTick, 11u);
    EXPECT_EQ(entry.snapshot.processCount, 2u);
    ASSERT_EQ(entry.snapshot.scheduler.runningQueue.size(), 2u);
    EXPECT_EQ(entry.snapshot.scheduler.runningQueue[0], 3u);
    EXPECT_EQ(entry.snapshot.scheduler.runningQueue[1], 7u);
}
