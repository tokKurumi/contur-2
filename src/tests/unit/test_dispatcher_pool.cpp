/// @file test_dispatcher_pool.cpp
/// @brief Unit tests for DispatcherPool runtime.

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>
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

            ProcessId pid = process->id();
            if (processes_.find(pid) != processes_.end())
            {
                return Result<void>::error(ErrorCode::AlreadyExists);
            }

            processes_.emplace(pid, std::move(process));
            return Result<void>::ok();
        }

        [[nodiscard]] Result<void> dispatch(std::size_t tickBudget) override
        {
            ++dispatchCalls_;
            lastTickBudget_ = tickBudget;
            if (dispatchHook_)
            {
                dispatchHook_();
            }
            return dispatchResult_;
        }

        [[nodiscard]] Result<void> terminateProcess(ProcessId pid, Tick currentTick) override
        {
            (void)currentTick;
            auto it = processes_.find(pid);
            if (it == processes_.end())
            {
                return Result<void>::error(ErrorCode::NotFound);
            }
            processes_.erase(it);
            return Result<void>::ok();
        }

        void tick() override
        {
            ++tickCalls_;
            if (tickHook_)
            {
                tickHook_();
            }
        }

        [[nodiscard]] std::size_t processCount() const noexcept override
        {
            return processes_.size();
        }

        [[nodiscard]] bool hasProcess(ProcessId pid) const noexcept override
        {
            return processes_.find(pid) != processes_.end();
        }

        void setDispatchResult(Result<void> result)
        {
            dispatchResult_ = result;
        }

        void setDispatchHook(std::function<void()> hook)
        {
            dispatchHook_ = std::move(hook);
        }

        void setTickHook(std::function<void()> hook)
        {
            tickHook_ = std::move(hook);
        }

        [[nodiscard]] std::size_t dispatchCalls() const noexcept
        {
            return dispatchCalls_;
        }

        [[nodiscard]] std::size_t tickCalls() const noexcept
        {
            return tickCalls_;
        }

        [[nodiscard]] std::size_t lastTickBudget() const noexcept
        {
            return lastTickBudget_;
        }

        private:
        std::unordered_map<ProcessId, std::unique_ptr<ProcessImage>> processes_;
        Result<void> dispatchResult_ = Result<void>::ok();
        std::function<void()> dispatchHook_;
        std::function<void()> tickHook_;
        std::size_t dispatchCalls_ = 0;
        std::size_t tickCalls_ = 0;
        std::size_t lastTickBudget_ = 0;
    };

    void trackMax(std::atomic<std::size_t> &maxValue, std::size_t candidate)
    {
        std::size_t observed = maxValue.load(std::memory_order_relaxed);
        while (candidate > observed)
        {
            if (maxValue.compare_exchange_weak(observed, candidate, std::memory_order_relaxed))
            {
                break;
            }
        }
    }

} // namespace

TEST(DispatcherPoolTest, ConfigIsNormalizedAndSingleThreadDisablesWorkStealing)
{
    DispatcherPool pool({.hostThreadCount = 0, .deterministicMode = false, .workStealingEnabled = true});

    EXPECT_EQ(pool.name(), "DispatcherPool");
    EXPECT_EQ(pool.config().hostThreadCount, 1u);
    EXPECT_TRUE(pool.config().isSingleThreaded());
    EXPECT_FALSE(pool.config().workStealingEnabled);
    EXPECT_FALSE(pool.config().deterministicMode);
}

TEST(DispatcherPoolTest, DispatchReturnsInvalidStateWhenNoLanes)
{
    DispatcherPool pool({.hostThreadCount = 2, .deterministicMode = true, .workStealingEnabled = false});
    DispatcherLanes lanes;

    auto result = pool.dispatch(lanes, 5);

    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidState);
}

TEST(DispatcherPoolTest, DispatchAggregatesLaneResultsLikeSerialRuntime)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    FakeDispatcher d2;

    d0.setDispatchResult(Result<void>::error(ErrorCode::NotFound));
    d1.setDispatchResult(Result<void>::ok());
    d2.setDispatchResult(Result<void>::error(ErrorCode::InvalidState));

    DispatcherPool pool({.hostThreadCount = 3, .deterministicMode = false, .workStealingEnabled = true});
    DispatcherLanes lanes{std::ref(d0), std::ref(d1), std::ref(d2)};

    auto result = pool.dispatch(lanes, 11);

    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(d0.dispatchCalls(), 1u);
    EXPECT_EQ(d1.dispatchCalls(), 1u);
    EXPECT_EQ(d2.dispatchCalls(), 1u);
    EXPECT_EQ(d0.lastTickBudget(), 11u);
    EXPECT_EQ(d1.lastTickBudget(), 11u);
    EXPECT_EQ(d2.lastTickBudget(), 11u);
}

TEST(DispatcherPoolTest, DispatchReturnsFirstNonNotFoundErrorWhenNoLaneSucceeds)
{
    FakeDispatcher d0;
    FakeDispatcher d1;

    d0.setDispatchResult(Result<void>::error(ErrorCode::ResourceBusy));
    d1.setDispatchResult(Result<void>::error(ErrorCode::NotFound));

    DispatcherPool pool({.hostThreadCount = 2, .deterministicMode = true, .workStealingEnabled = false});
    DispatcherLanes lanes{std::ref(d0), std::ref(d1)};

    auto result = pool.dispatch(lanes, 3);

    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::ResourceBusy);
}

TEST(DispatcherPoolTest, TickBroadcastsAcrossAllLanes)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    FakeDispatcher d2;

    DispatcherPool pool({.hostThreadCount = 2, .deterministicMode = false, .workStealingEnabled = true});
    DispatcherLanes lanes{std::ref(d0), std::ref(d1), std::ref(d2)};

    pool.tick(lanes);

    EXPECT_EQ(d0.tickCalls(), 1u);
    EXPECT_EQ(d1.tickCalls(), 1u);
    EXPECT_EQ(d2.tickCalls(), 1u);
}

TEST(DispatcherPoolTest, DeterministicModeKeepsStableLaneOwnershipAcrossDispatches)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    FakeDispatcher d2;
    FakeDispatcher d3;

    std::array<std::thread::id, 4> firstPass{};
    std::array<std::thread::id, 4> secondPass{};

    d0.setDispatchHook([&firstPass] { firstPass[0] = std::this_thread::get_id(); });
    d1.setDispatchHook([&firstPass] { firstPass[1] = std::this_thread::get_id(); });
    d2.setDispatchHook([&firstPass] { firstPass[2] = std::this_thread::get_id(); });
    d3.setDispatchHook([&firstPass] { firstPass[3] = std::this_thread::get_id(); });

    DispatcherPool pool({.hostThreadCount = 2, .deterministicMode = true, .workStealingEnabled = true});
    DispatcherLanes lanes{std::ref(d0), std::ref(d1), std::ref(d2), std::ref(d3)};

    ASSERT_TRUE(pool.dispatch(lanes, 7).isOk());

    d0.setDispatchHook([&secondPass] { secondPass[0] = std::this_thread::get_id(); });
    d1.setDispatchHook([&secondPass] { secondPass[1] = std::this_thread::get_id(); });
    d2.setDispatchHook([&secondPass] { secondPass[2] = std::this_thread::get_id(); });
    d3.setDispatchHook([&secondPass] { secondPass[3] = std::this_thread::get_id(); });

    ASSERT_TRUE(pool.dispatch(lanes, 9).isOk());

    EXPECT_EQ(firstPass[0], secondPass[0]);
    EXPECT_EQ(firstPass[1], secondPass[1]);
    EXPECT_EQ(firstPass[2], secondPass[2]);
    EXPECT_EQ(firstPass[3], secondPass[3]);
}

TEST(DispatcherPoolTest, MultiThreadedDispatchShowsParallelLaneExecution)
{
    std::atomic<std::size_t> active{0};
    std::atomic<std::size_t> maxActive{0};

    std::vector<std::unique_ptr<FakeDispatcher>> ownedLanes;
    DispatcherLanes lanes;
    ownedLanes.reserve(4);
    lanes.reserve(4);

    for (std::size_t i = 0; i < 4; ++i)
    {
        auto lane = std::make_unique<FakeDispatcher>();
        lane->setDispatchHook([&active, &maxActive] {
            std::size_t nowActive = active.fetch_add(1, std::memory_order_relaxed) + 1;
            trackMax(maxActive, nowActive);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            active.fetch_sub(1, std::memory_order_relaxed);
        });
        lanes.push_back(std::ref(*lane));
        ownedLanes.push_back(std::move(lane));
    }

    DispatcherPool pool({.hostThreadCount = 4, .deterministicMode = false, .workStealingEnabled = true});

    auto result = pool.dispatch(lanes, 13);

    ASSERT_TRUE(result.isOk());
    EXPECT_GE(maxActive.load(std::memory_order_relaxed), 2u);
}
