/// @file test_tui_history_buffer_extended.cpp
/// @brief Extended unit tests for HistoryBuffer — capacity edge cases,
///        moveToLatest, empty-buffer guards, repeated wraparound, capacity=1.

#include <gtest/gtest.h>

#include "contur/core/error.h"

#include "contur/tui/history_buffer.h"

using namespace contur;

namespace {

    TuiHistoryEntry makeEntry(std::size_t sequence, Tick tick)
    {
        TuiHistoryEntry entry;
        entry.sequence = sequence;
        entry.snapshot.sequence = static_cast<std::uint64_t>(sequence);
        entry.snapshot.currentTick = tick;
        return entry;
    }

} // namespace

// Empty buffer guards
TEST(HistoryBufferExtTest, EmptyBufferHasNoCurrentOrLatest)
{
    HistoryBuffer buf(4);
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0u);
    EXPECT_FALSE(buf.current().has_value());
    EXPECT_FALSE(buf.latest().has_value());
}

TEST(HistoryBufferExtTest, SeekBackwardOnEmptyReturnsNotFound)
{
    HistoryBuffer buf(4);
    auto r = buf.seekBackward(1);
    ASSERT_TRUE(r.isError());
    EXPECT_EQ(r.errorCode(), ErrorCode::NotFound);
}

TEST(HistoryBufferExtTest, SeekForwardOnEmptyReturnsNotFound)
{
    HistoryBuffer buf(4);
    auto r = buf.seekForward(1);
    ASSERT_TRUE(r.isError());
    EXPECT_EQ(r.errorCode(), ErrorCode::NotFound);
}

TEST(HistoryBufferExtTest, MoveToLatestOnEmptyBufferIsNoOp)
{
    HistoryBuffer buf(4);
    buf.moveToLatest(); // must not crash
    EXPECT_TRUE(buf.empty());
}

// Capacity = 1 edge case
TEST(HistoryBufferExtTest, CapacityOneRetainsOnlyLatestEntry)
{
    HistoryBuffer buf(1);

    ASSERT_TRUE(buf.append(makeEntry(1, 10)).isOk());
    ASSERT_TRUE(buf.append(makeEntry(2, 20)).isOk());
    ASSERT_TRUE(buf.append(makeEntry(3, 30)).isOk());

    EXPECT_EQ(buf.size(), 1u);
    ASSERT_TRUE(buf.current().has_value());
    EXPECT_EQ(buf.current()->get().sequence, 3u);
    ASSERT_TRUE(buf.latest().has_value());
    EXPECT_EQ(buf.latest()->get().snapshot.currentTick, 30u);
}

TEST(HistoryBufferExtTest, CapacityOneSeekAlwaysFails)
{
    HistoryBuffer buf(1);
    ASSERT_TRUE(buf.append(makeEntry(1, 10)).isOk());

    EXPECT_TRUE(buf.seekBackward(1).isError());
    EXPECT_TRUE(buf.seekForward(1).isError());
}

// Capacity normalisation (0 → 1)
TEST(HistoryBufferExtTest, ZeroCapacityNormalisesToOne)
{
    HistoryBuffer buf(0);
    EXPECT_GE(buf.capacity(), 1u);

    ASSERT_TRUE(buf.append(makeEntry(1, 5)).isOk());
    ASSERT_TRUE(buf.append(makeEntry(2, 6)).isOk()); // second append still ok

    EXPECT_EQ(buf.size(), 1u);
    EXPECT_EQ(buf.current()->get().sequence, 2u);
}

// moveToLatest
TEST(HistoryBufferExtTest, MoveToLatestAfterSeekJumpsToNewest)
{
    HistoryBuffer buf(5);
    ASSERT_TRUE(buf.append(makeEntry(1, 10)).isOk());
    ASSERT_TRUE(buf.append(makeEntry(2, 20)).isOk());
    ASSERT_TRUE(buf.append(makeEntry(3, 30)).isOk());

    ASSERT_TRUE(buf.seekBackward(2).isOk());
    EXPECT_EQ(buf.current()->get().sequence, 1u);

    buf.moveToLatest();
    EXPECT_EQ(buf.current()->get().sequence, 3u);
    EXPECT_EQ(buf.cursor(), buf.size() - 1);
}

// Repeated wraparound
TEST(HistoryBufferExtTest, RepeatedWraparoundAlwaysRetainsMostRecentN)
{
    const std::size_t cap = 3;
    HistoryBuffer buf(cap);

    for (std::size_t i = 1; i <= 10; ++i)
    {
        ASSERT_TRUE(buf.append(makeEntry(i, i * 10)).isOk());
    }

    EXPECT_EQ(buf.size(), cap);
    // oldest retained: entry 8 (10 - 3 + 1)
    ASSERT_TRUE(buf.seekBackward(cap - 1).isOk());
    EXPECT_EQ(buf.current()->get().sequence, 8u);
}

TEST(HistoryBufferExtTest, CapacityQueryReflectsConstructorArgument)
{
    HistoryBuffer buf(7);
    EXPECT_EQ(buf.capacity(), 7u);
}

// latest() vs current()
TEST(HistoryBufferExtTest, LatestAlwaysPointsToNewestRegardlessOfCursor)
{
    HistoryBuffer buf(5);
    ASSERT_TRUE(buf.append(makeEntry(1, 100)).isOk());
    ASSERT_TRUE(buf.append(makeEntry(2, 200)).isOk());
    ASSERT_TRUE(buf.append(makeEntry(3, 300)).isOk());

    // Seek back so current != latest
    ASSERT_TRUE(buf.seekBackward(2).isOk());
    ASSERT_TRUE(buf.current().has_value());
    ASSERT_TRUE(buf.latest().has_value());

    EXPECT_EQ(buf.current()->get().sequence, 1u);
    EXPECT_EQ(buf.latest()->get().sequence, 3u);
}

// Cursor stays at latest after each append
TEST(HistoryBufferExtTest, CursorPointsToNewestAfterEachAppend)
{
    HistoryBuffer buf(4);
    for (std::size_t i = 1; i <= 4; ++i)
    {
        ASSERT_TRUE(buf.append(makeEntry(i, i)).isOk());
        EXPECT_EQ(buf.cursor(), buf.size() - 1) << "after append " << i;
        EXPECT_EQ(buf.current()->get().sequence, i);
    }
}

// Seek step larger than available history
TEST(HistoryBufferExtTest, SeekBackwardMoreThanAvailableReturnsNotFound)
{
    HistoryBuffer buf(5);
    ASSERT_TRUE(buf.append(makeEntry(1, 1)).isOk());
    ASSERT_TRUE(buf.append(makeEntry(2, 2)).isOk());

    auto r = buf.seekBackward(5); // only 2 entries
    ASSERT_TRUE(r.isError());
    EXPECT_EQ(r.errorCode(), ErrorCode::NotFound);
}

TEST(HistoryBufferExtTest, SeekForwardFromLatestReturnsNotFound)
{
    HistoryBuffer buf(5);
    ASSERT_TRUE(buf.append(makeEntry(1, 1)).isOk());

    // already at latest
    auto r = buf.seekForward(1);
    ASSERT_TRUE(r.isError());
    EXPECT_EQ(r.errorCode(), ErrorCode::NotFound);
}
