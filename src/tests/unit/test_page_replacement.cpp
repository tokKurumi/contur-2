/// @file test_page_replacement.cpp
/// @brief Unit tests for page replacement algorithms (FIFO, LRU, Clock, Optimal).

#include <gtest/gtest.h>

#include "contur/memory/clock_replacement.h"
#include "contur/memory/fifo_replacement.h"
#include "contur/memory/lru_replacement.h"
#include "contur/memory/optimal_replacement.h"
#include "contur/memory/page_table.h"

using namespace contur;

// ──────────────────────────────── FIFO ────────────────────────────────

TEST(FifoReplacementTest, NameIsFIFO)
{
    FifoReplacement fifo;
    EXPECT_EQ(fifo.name(), "FIFO");
}

TEST(FifoReplacementTest, SelectVictimReturnsOldestLoaded)
{
    FifoReplacement fifo;
    PageTable pt(4);

    fifo.onLoad(10);
    fifo.onLoad(20);
    fifo.onLoad(30);

    EXPECT_EQ(fifo.selectVictim(pt), 10u);
    EXPECT_EQ(fifo.selectVictim(pt), 20u);
    EXPECT_EQ(fifo.selectVictim(pt), 30u);
}

TEST(FifoReplacementTest, SelectVictimEmptyReturnsInvalid)
{
    FifoReplacement fifo;
    PageTable pt(4);
    EXPECT_EQ(fifo.selectVictim(pt), INVALID_FRAME);
}

TEST(FifoReplacementTest, AccessDoesNotChangeOrder)
{
    FifoReplacement fifo;
    PageTable pt(4);

    fifo.onLoad(1);
    fifo.onLoad(2);
    fifo.onLoad(3);

    // Access frame 1 — shouldn't change FIFO order
    fifo.onAccess(1);

    EXPECT_EQ(fifo.selectVictim(pt), 1u);
}

TEST(FifoReplacementTest, Reset)
{
    FifoReplacement fifo;
    PageTable pt(4);

    fifo.onLoad(1);
    fifo.onLoad(2);
    fifo.reset();

    EXPECT_EQ(fifo.selectVictim(pt), INVALID_FRAME);
}

// ──────────────────────────────── LRU ────────────────────────────────

TEST(LruReplacementTest, NameIsLRU)
{
    LruReplacement lru;
    EXPECT_EQ(lru.name(), "LRU");
}

TEST(LruReplacementTest, SelectVictimReturnsLeastRecentlyUsed)
{
    LruReplacement lru;
    PageTable pt(4);

    lru.onLoad(10);
    lru.onLoad(20);
    lru.onLoad(30);

    // Access 10 again — now 20 is the LRU
    lru.onAccess(10);

    EXPECT_EQ(lru.selectVictim(pt), 20u);
}

TEST(LruReplacementTest, SelectVictimEmptyReturnsInvalid)
{
    LruReplacement lru;
    PageTable pt(4);
    EXPECT_EQ(lru.selectVictim(pt), INVALID_FRAME);
}

TEST(LruReplacementTest, AccessUpdatesOrder)
{
    LruReplacement lru;
    PageTable pt(4);

    lru.onLoad(1);
    lru.onLoad(2);
    lru.onLoad(3);

    // Access frames: 1 is now most recent, 2 is least recent
    lru.onAccess(3);
    lru.onAccess(1);

    EXPECT_EQ(lru.selectVictim(pt), 2u);
}

TEST(LruReplacementTest, Reset)
{
    LruReplacement lru;
    PageTable pt(4);

    lru.onLoad(1);
    lru.onLoad(2);
    lru.reset();

    EXPECT_EQ(lru.selectVictim(pt), INVALID_FRAME);
}

// ──────────────────────────────── Clock ────────────────────────────────

TEST(ClockReplacementTest, NameIsClock)
{
    ClockReplacement clock;
    EXPECT_EQ(clock.name(), "Clock");
}

TEST(ClockReplacementTest, SelectVictimWithSecondChance)
{
    ClockReplacement clock;
    PageTable pt(4);

    clock.onLoad(1); // ref=1
    clock.onLoad(2); // ref=1
    clock.onLoad(3); // ref=1

    // Clear ref bits by not accessing — but onLoad sets ref bit,
    // so first pass clears all ref bits, second pass evicts first.
    // Frame 1 gets second chance, then loses ref bit.
    // Frame 2 gets second chance, then loses ref bit.
    // Frame 3 gets second chance, then loses ref bit.
    // Then frame 1 is evicted (no ref bit on second pass).
    FrameId victim = clock.selectVictim(pt);
    EXPECT_EQ(victim, 1u);
}

TEST(ClockReplacementTest, SelectVictimEmptyReturnsInvalid)
{
    ClockReplacement clock;
    PageTable pt(4);
    EXPECT_EQ(clock.selectVictim(pt), INVALID_FRAME);
}

TEST(ClockReplacementTest, AccessedFrameGetsSecondChance)
{
    ClockReplacement clock;
    PageTable pt(4);

    clock.onLoad(1); // ref=1
    clock.onLoad(2); // ref=1
    clock.onLoad(3); // ref=1

    // First victim eviction clears all ref bits, then evicts 1
    FrameId first = clock.selectVictim(pt);
    EXPECT_EQ(first, 1u);

    // Now 2 and 3 have ref=0. Access 2 to give it ref=1.
    clock.onAccess(2);

    // Next eviction: hand is at 2, ref=1 → clear, move to 3 (ref=0) → evict 3
    FrameId second = clock.selectVictim(pt);
    EXPECT_EQ(second, 3u);
}

TEST(ClockReplacementTest, Reset)
{
    ClockReplacement clock;
    PageTable pt(4);

    clock.onLoad(1);
    clock.onLoad(2);
    clock.reset();

    EXPECT_EQ(clock.selectVictim(pt), INVALID_FRAME);
}

// ──────────────────────────────── Optimal ────────────────────────────────

TEST(OptimalReplacementTest, NameIsOptimal)
{
    OptimalReplacement opt({});
    EXPECT_EQ(opt.name(), "Optimal");
}

TEST(OptimalReplacementTest, SelectVictimEvictsNeverUsedAgain)
{
    // Future accesses: frame 1, frame 2, frame 1, frame 3
    OptimalReplacement opt({1, 2, 1, 3});
    PageTable pt(4);

    opt.onLoad(1);
    opt.onLoad(2);
    opt.onLoad(3);

    // From index 0: frame 1 next at 0, frame 2 next at 1, frame 3 next at 3
    // Frame 3 is farthest → evict 3
    FrameId victim = opt.selectVictim(pt);
    EXPECT_EQ(victim, 3u);
}

TEST(OptimalReplacementTest, SelectVictimEvictsFrameNotInFuture)
{
    // Future: only frames 1 and 2 appear again
    OptimalReplacement opt({1, 2, 1});
    PageTable pt(4);

    opt.onLoad(1);
    opt.onLoad(2);
    opt.onLoad(3); // Frame 3 never appears in future → evict immediately

    FrameId victim = opt.selectVictim(pt);
    EXPECT_EQ(victim, 3u);
}

TEST(OptimalReplacementTest, SelectVictimEmptyReturnsInvalid)
{
    OptimalReplacement opt({});
    PageTable pt(4);
    EXPECT_EQ(opt.selectVictim(pt), INVALID_FRAME);
}

TEST(OptimalReplacementTest, Reset)
{
    OptimalReplacement opt({1, 2, 3});
    PageTable pt(4);

    opt.onLoad(1);
    opt.onAccess(1);
    opt.reset();

    EXPECT_EQ(opt.selectVictim(pt), INVALID_FRAME);
}
