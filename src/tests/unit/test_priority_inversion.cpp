/// @file test_priority_inversion.cpp
/// @brief Tests for priority inheritance/boost behavior in sync primitives.

#include <gtest/gtest.h>

#include "contur/process/priority.h"
#include "contur/sync/mutex.h"
#include "contur/sync/semaphore.h"

using namespace contur;

TEST(PriorityInversionTest, MutexBoostsOwnerWhileHighPriorityWaiterBlocks)
{
    Mutex mutex;

    ASSERT_TRUE(mutex.registerProcessPriority(1, PriorityLevel::Low).isOk());
    ASSERT_TRUE(mutex.registerProcessPriority(2, PriorityLevel::High).isOk());

    ASSERT_TRUE(mutex.acquire(1).isOk());

    auto busy = mutex.acquire(2);
    ASSERT_TRUE(busy.isError());
    EXPECT_EQ(busy.errorCode(), ErrorCode::ResourceBusy);

    EXPECT_EQ(mutex.basePriority(1), PriorityLevel::Low);
    EXPECT_EQ(mutex.effectivePriority(1), PriorityLevel::High);

    ASSERT_TRUE(mutex.release(1).isOk());
    EXPECT_EQ(mutex.owner(), std::optional<ProcessId>(2));
    EXPECT_EQ(mutex.effectivePriority(1), PriorityLevel::Low);
}

TEST(PriorityInversionTest, SemaphoreBoostsHolderWhenHighPriorityWaiterBlocks)
{
    Semaphore semaphore(1, 1);

    ASSERT_TRUE(semaphore.registerProcessPriority(1, PriorityLevel::Low).isOk());
    ASSERT_TRUE(semaphore.registerProcessPriority(2, PriorityLevel::High).isOk());

    ASSERT_TRUE(semaphore.acquire(1).isOk());

    auto busy = semaphore.acquire(2);
    ASSERT_TRUE(busy.isError());
    EXPECT_EQ(busy.errorCode(), ErrorCode::ResourceBusy);

    EXPECT_EQ(semaphore.basePriority(1), PriorityLevel::Low);
    EXPECT_EQ(semaphore.effectivePriority(1), PriorityLevel::High);

    ASSERT_TRUE(semaphore.release(1).isOk());
    EXPECT_EQ(semaphore.effectivePriority(1), PriorityLevel::Low);
}

TEST(PriorityInversionTest, TryAcquireDoesNotApplyBoostWhenNoWaitQueueEntry)
{
    Semaphore semaphore(1, 1);

    ASSERT_TRUE(semaphore.registerProcessPriority(1, PriorityLevel::Low).isOk());
    ASSERT_TRUE(semaphore.registerProcessPriority(2, PriorityLevel::High).isOk());
    ASSERT_TRUE(semaphore.acquire(1).isOk());

    auto busy = semaphore.tryAcquire(2);
    ASSERT_TRUE(busy.isError());
    EXPECT_EQ(busy.errorCode(), ErrorCode::ResourceBusy);

    EXPECT_EQ(semaphore.effectivePriority(1), PriorityLevel::Low);
}
