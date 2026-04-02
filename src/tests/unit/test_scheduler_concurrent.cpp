/// @file test_scheduler_concurrent.cpp
/// @brief Unit tests for lane-aware concurrent scheduler behavior.

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "contur/core/clock.h"

#include "contur/process/pcb.h"
#include "contur/scheduling/fcfs_policy.h"
#include "contur/scheduling/scheduler.h"

using namespace contur;

TEST(SchedulerConcurrentTest, ConfigureLanesValidatesArgumentsAndState)
{
    Scheduler scheduler(std::make_unique<FcfsPolicy>());

    auto zero = scheduler.configureLanes(0);
    ASSERT_TRUE(zero.isError());
    EXPECT_EQ(zero.errorCode(), ErrorCode::InvalidArgument);

    PCB p1(1, "p1");
    ASSERT_TRUE(scheduler.enqueue(p1, 0).isOk());

    auto busy = scheduler.configureLanes(2);
    ASSERT_TRUE(busy.isError());
    EXPECT_EQ(busy.errorCode(), ErrorCode::InvalidState);
}

TEST(SchedulerConcurrentTest, EnqueueToLaneCreatesIndependentPerLaneQueues)
{
    Scheduler scheduler(std::make_unique<FcfsPolicy>());
    ASSERT_TRUE(scheduler.configureLanes(3).isOk());

    PCB p1(1, "p1");
    PCB p2(2, "p2");
    PCB p3(3, "p3");

    ASSERT_TRUE(scheduler.enqueueToLane(p1, 0, 0).isOk());
    ASSERT_TRUE(scheduler.enqueueToLane(p2, 1, 0).isOk());
    ASSERT_TRUE(scheduler.enqueueToLane(p3, 2, 0).isOk());

    auto lanes = scheduler.getPerLaneQueueSnapshot();
    ASSERT_EQ(lanes.size(), 3u);
    ASSERT_EQ(lanes[0].size(), 1u);
    ASSERT_EQ(lanes[1].size(), 1u);
    ASSERT_EQ(lanes[2].size(), 1u);
    EXPECT_EQ(lanes[0][0], 1u);
    EXPECT_EQ(lanes[1][0], 2u);
    EXPECT_EQ(lanes[2][0], 3u);
}

TEST(SchedulerConcurrentTest, SelectNextForLaneTracksMultipleRunningProcesses)
{
    SimulationClock clock;
    Scheduler scheduler(std::make_unique<FcfsPolicy>());
    ASSERT_TRUE(scheduler.configureLanes(2).isOk());

    PCB p1(1, "p1");
    PCB p2(2, "p2");

    ASSERT_TRUE(scheduler.enqueueToLane(p1, 0, 0).isOk());
    ASSERT_TRUE(scheduler.enqueueToLane(p2, 1, 0).isOk());

    auto lane0 = scheduler.selectNextForLane(0, clock);
    auto lane1 = scheduler.selectNextForLane(1, clock);

    ASSERT_TRUE(lane0.isOk());
    ASSERT_TRUE(lane1.isOk());
    EXPECT_EQ(lane0.value(), 1u);
    EXPECT_EQ(lane1.value(), 2u);

    auto running = scheduler.runningProcesses();
    std::sort(running.begin(), running.end());
    ASSERT_EQ(running.size(), 2u);
    EXPECT_EQ(running[0], 1u);
    EXPECT_EQ(running[1], 2u);
}

TEST(SchedulerConcurrentTest, StealMovesWorkToThiefLaneAndMaintainsUniqueness)
{
    SimulationClock clock;
    Scheduler scheduler(std::make_unique<FcfsPolicy>());
    ASSERT_TRUE(scheduler.configureLanes(2).isOk());

    PCB p1(1, "p1");
    PCB p2(2, "p2");

    ASSERT_TRUE(scheduler.enqueueToLane(p1, 0, 0).isOk());
    ASSERT_TRUE(scheduler.enqueueToLane(p2, 0, 0).isOk());

    auto stolen = scheduler.stealNextForLane(1, clock);
    ASSERT_TRUE(stolen.isOk());

    auto lanes = scheduler.getPerLaneQueueSnapshot();
    ASSERT_EQ(lanes.size(), 2u);

    auto running = scheduler.runningProcesses();
    ASSERT_EQ(running.size(), 1u);

    std::vector<ProcessId> all;
    for (const auto &lane : lanes)
    {
        all.insert(all.end(), lane.begin(), lane.end());
    }
    all.insert(all.end(), running.begin(), running.end());

    std::sort(all.begin(), all.end());
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0], 1u);
    EXPECT_EQ(all[1], 2u);
}

// ---------------------------------------------------------------------------
// Real-thread concurrency tests
// ---------------------------------------------------------------------------

/// Enqueue 200 unique processes from 4 threads simultaneously and verify that
/// all PIDs end up in the scheduler exactly once with no data races.
TEST(SchedulerConcurrentTest, ConcurrentEnqueueFromMultipleThreads)
{
    constexpr int kThreads = 4;
    constexpr int kPerThread = 50; // 200 processes total

    Scheduler scheduler(std::make_unique<FcfsPolicy>());
    ASSERT_TRUE(scheduler.configureLanes(kThreads).isOk());

    // Pre-allocate PCBs so threads only enqueue (no allocations under lock).
    std::vector<PCB> pcbs;
    pcbs.reserve(static_cast<std::size_t>(kThreads * kPerThread));
    for (int i = 1; i <= kThreads * kPerThread; ++i)
    {
        pcbs.emplace_back(static_cast<ProcessId>(i), "p" + std::to_string(i));
    }

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(kThreads));

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&, t]() {
            const int base = t * kPerThread;
            for (int i = 0; i < kPerThread; ++i)
            {
                auto res =
                    scheduler.enqueueToLane(pcbs[static_cast<std::size_t>(base + i)], static_cast<std::size_t>(t), 0);
                EXPECT_TRUE(res.isOk());
            }
        });
    }

    for (auto &th : threads)
    {
        th.join();
    }

    // Verify total count and uniqueness across all lanes.
    auto lanes = scheduler.getPerLaneQueueSnapshot();
    ASSERT_EQ(lanes.size(), static_cast<std::size_t>(kThreads));

    std::vector<ProcessId> all;
    for (const auto &lane : lanes)
    {
        all.insert(all.end(), lane.begin(), lane.end());
    }

    ASSERT_EQ(all.size(), static_cast<std::size_t>(kThreads * kPerThread));

    std::sort(all.begin(), all.end());
    for (int i = 0; i < kThreads * kPerThread; ++i)
    {
        EXPECT_EQ(all[static_cast<std::size_t>(i)], static_cast<ProcessId>(i + 1));
    }
}

/// Concurrently enqueue to lane 0 while lane 1 repeatedly steals.
/// Verifies that no process is duplicated or lost under concurrent steal+enqueue.
TEST(SchedulerConcurrentTest, ConcurrentStealUnderConcurrentEnqueue)
{
    constexpr int kEnqueueCount = 80;

    Scheduler scheduler(std::make_unique<FcfsPolicy>());
    ASSERT_TRUE(scheduler.configureLanes(2).isOk());

    std::vector<PCB> pcbs;
    pcbs.reserve(kEnqueueCount);
    for (int i = 1; i <= kEnqueueCount; ++i)
    {
        pcbs.emplace_back(static_cast<ProcessId>(i), "p" + std::to_string(i));
    }

    SimulationClock clock;
    std::atomic<int> stolenCount{0};
    std::atomic<bool> start{false};
    std::atomic<bool> producerDone{false};

    // Thief thread: keeps stealing from lane 0 into lane 1.
    std::thread thief([&]() {
        while (!start.load())
        {
            std::this_thread::yield();
        }

        int attempt = 0;
        while (!producerDone.load() || attempt < kEnqueueCount * 2)
        {
            auto res = scheduler.stealNextForLane(1, clock);
            if (res.isOk())
            {
                stolenCount.fetch_add(1, std::memory_order_relaxed);
            }
            ++attempt;
            std::this_thread::yield();
        }
    });

    // Producer thread: enqueues all processes into lane 0.
    std::thread producer([&]() {
        start.store(true);
        for (int i = 0; i < kEnqueueCount; ++i)
        {
            auto res = scheduler.enqueueToLane(pcbs[static_cast<std::size_t>(i)], 0, static_cast<Tick>(i));
            EXPECT_TRUE(res.isOk());
        }
        producerDone.store(true);
    });

    producer.join();
    thief.join();

    // All processes must be accounted for exactly once across ready queues + running.
    auto lanes = scheduler.getPerLaneQueueSnapshot();
    auto running = scheduler.runningProcesses();

    std::vector<ProcessId> all;
    for (const auto &lane : lanes)
    {
        all.insert(all.end(), lane.begin(), lane.end());
    }
    all.insert(all.end(), running.begin(), running.end());

    ASSERT_EQ(all.size(), static_cast<std::size_t>(kEnqueueCount));

    std::sort(all.begin(), all.end());
    for (int i = 0; i < kEnqueueCount; ++i)
    {
        EXPECT_EQ(all[static_cast<std::size_t>(i)], static_cast<ProcessId>(i + 1));
    }

    // At least some steals should have succeeded (probabilistic liveness check).
    EXPECT_GT(stolenCount.load(), 0);
}
