/// @file test_tracer_concurrent.cpp
/// @brief Concurrent runtime trace metadata tests.

#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "contur/dispatch/dispatcher_pool.h"
#include "contur/dispatch/i_dispatcher.h"
#include "contur/process/process_image.h"

using namespace contur;

namespace {

    class FakeDispatcher final : public IDispatcher
    {
        public:
        [[nodiscard]] Result<void> createProcess(std::unique_ptr<ProcessImage> process, Tick currentTick) override
        {
            (void)currentTick;
            if (!process)
            {
                return Result<void>::error(ErrorCode::InvalidArgument);
            }
            return Result<void>::ok();
        }

        [[nodiscard]] Result<void> dispatch(std::size_t tickBudget) override
        {
            (void)tickBudget;
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            return Result<void>::ok();
        }

        [[nodiscard]] Result<void> terminateProcess(ProcessId pid, Tick currentTick) override
        {
            (void)pid;
            (void)currentTick;
            return Result<void>::ok();
        }

        void tick() override
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        [[nodiscard]] std::size_t processCount() const noexcept override
        {
            return 0;
        }

        [[nodiscard]] bool hasProcess(ProcessId pid) const noexcept override
        {
            (void)pid;
            return false;
        }
    };

} // namespace

TEST(TracerConcurrentTest, DispatcherPoolDispatchEmitsWorkerSequenceEpochMetadata)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    FakeDispatcher d2;
    FakeDispatcher d3;

    DispatcherPool pool({.hostThreadCount = 4, .deterministicMode = false, .workStealingEnabled = true});
    DispatcherLanes lanes{std::ref(d0), std::ref(d1), std::ref(d2), std::ref(d3)};

    ASSERT_TRUE(pool.dispatch(lanes, 9).isOk());

    auto events = pool.lastTraceEvents();
    auto epoch = pool.lastEpoch();

    ASSERT_EQ(events.size(), lanes.size());

    std::vector<std::uint64_t> sequences;
    sequences.reserve(events.size());

    for (const auto &event : events)
    {
        EXPECT_EQ(event.subsystem, "dispatch.runtime");
        EXPECT_EQ(event.operation, "dispatch");
        EXPECT_EQ(event.epoch, epoch);
        EXPECT_LT(event.workerId, 4u);
        sequences.push_back(event.sequence);
    }

    std::sort(sequences.begin(), sequences.end());
    for (std::size_t i = 0; i < sequences.size(); ++i)
    {
        EXPECT_EQ(sequences[i], i);
    }
}

TEST(TracerConcurrentTest, DispatcherPoolTickUpdatesEpochAndTraceMetadata)
{
    FakeDispatcher d0;
    FakeDispatcher d1;

    DispatcherPool pool({.hostThreadCount = 2, .deterministicMode = false, .workStealingEnabled = true});
    DispatcherLanes lanes{std::ref(d0), std::ref(d1)};

    ASSERT_TRUE(pool.dispatch(lanes, 3).isOk());
    auto dispatchEpoch = pool.lastEpoch();

    pool.tick(lanes);
    auto tickEpoch = pool.lastEpoch();
    auto events = pool.lastTraceEvents();

    EXPECT_GT(tickEpoch, dispatchEpoch);
    ASSERT_EQ(events.size(), lanes.size());
    for (const auto &event : events)
    {
        EXPECT_EQ(event.operation, "tick");
        EXPECT_EQ(event.epoch, tickEpoch);
    }
}
