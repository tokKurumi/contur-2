/// @file test_deterministic_multithread.cpp
/// @brief Integration tests for deterministic N>1 dispatcher runtime replay behavior.

#include <memory>
#include <string>
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
            ++dispatchCalls_;
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
            ++tickCalls_;
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

        [[nodiscard]] std::size_t dispatchCalls() const noexcept
        {
            return dispatchCalls_;
        }

        [[nodiscard]] std::size_t tickCalls() const noexcept
        {
            return tickCalls_;
        }

        private:
        std::size_t dispatchCalls_ = 0;
        std::size_t tickCalls_ = 0;
    };

} // namespace

TEST(DeterministicMultithreadIntegrationTest, DeterministicDispatchHasStableOrderAndEpochProgression)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    FakeDispatcher d2;
    FakeDispatcher d3;

    DispatcherPool pool({.hostThreadCount = 4, .deterministicMode = true, .workStealingEnabled = true});
    DispatcherLanes lanes{std::ref(d0), std::ref(d1), std::ref(d2), std::ref(d3)};

    ASSERT_TRUE(pool.dispatch(lanes, 5).isOk());
    auto order1 = pool.lastDeterministicOrder();
    auto trace1 = pool.lastTraceEvents();
    auto epoch1 = pool.lastEpoch();

    ASSERT_TRUE(pool.dispatch(lanes, 7).isOk());
    auto order2 = pool.lastDeterministicOrder();
    auto trace2 = pool.lastTraceEvents();
    auto epoch2 = pool.lastEpoch();

    std::vector<std::size_t> expectedOrder{0, 1, 2, 3};
    EXPECT_EQ(order1, expectedOrder);
    EXPECT_EQ(order2, expectedOrder);

    EXPECT_EQ(epoch2, epoch1 + 1);

    ASSERT_EQ(trace1.size(), lanes.size());
    ASSERT_EQ(trace2.size(), lanes.size());

    for (std::size_t i = 0; i < trace1.size(); ++i)
    {
        EXPECT_EQ(trace1[i].operation, "dispatch");
        EXPECT_EQ(trace1[i].details, "lane=" + std::to_string(i));
        EXPECT_EQ(trace1[i].sequence, i);
        EXPECT_EQ(trace1[i].epoch, epoch1);
    }

    for (std::size_t i = 0; i < trace2.size(); ++i)
    {
        EXPECT_EQ(trace2[i].operation, "dispatch");
        EXPECT_EQ(trace2[i].details, "lane=" + std::to_string(i));
        EXPECT_EQ(trace2[i].sequence, i);
        EXPECT_EQ(trace2[i].epoch, epoch2);
    }

    EXPECT_EQ(d0.dispatchCalls(), 2u);
    EXPECT_EQ(d1.dispatchCalls(), 2u);
    EXPECT_EQ(d2.dispatchCalls(), 2u);
    EXPECT_EQ(d3.dispatchCalls(), 2u);
}
