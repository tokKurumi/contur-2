/// @file test_mp_dispatcher.cpp
/// @brief Unit tests for MPDispatcher.

#include <functional>
#include <memory>
#include <set>
#include <vector>

#include <gtest/gtest.h>

#include "contur/dispatch/i_dispatch_runtime.h"
#include "contur/dispatch/i_dispatcher.h"
#include "contur/dispatch/mp_dispatcher.h"
#include "contur/dispatch/serial_dispatch_runtime.h"
#include "contur/process/process_image.h"

using namespace contur;

namespace {

    class FakeDispatcher final : public IDispatcher
    {
        public:
        Result<void> createProcess(std::unique_ptr<ProcessImage> process, Tick currentTick) override
        {
            (void)currentTick;
            if (!process)
            {
                return Result<void>::error(ErrorCode::InvalidArgument);
            }
            const ProcessId pid = process->id();
            knownPids_.insert(pid);
            createdPids_.push_back(pid);
            return createResult_;
        }

        Result<void> dispatch(std::size_t tickBudget) override
        {
            lastTickBudget_ = tickBudget;
            return dispatchResult_;
        }

        Result<void> terminateProcess(ProcessId pid, Tick currentTick) override
        {
            (void)currentTick;
            if (knownPids_.find(pid) == knownPids_.end())
            {
                return Result<void>::error(ErrorCode::NotFound);
            }
            if (terminateResult_.isError())
            {
                return terminateResult_;
            }
            knownPids_.erase(pid);
            terminatedPids_.push_back(pid);
            return Result<void>::ok();
        }

        void tick() override
        {
            ++tickCalls_;
        }

        std::size_t processCount() const noexcept override
        {
            return knownPids_.size();
        }

        bool hasProcess(ProcessId pid) const noexcept override
        {
            return knownPids_.find(pid) != knownPids_.end();
        }

        void setCreateResult(Result<void> result)
        {
            createResult_ = result;
        }

        void setDispatchResult(Result<void> result)
        {
            dispatchResult_ = result;
        }

        void setTerminateResult(Result<void> result)
        {
            terminateResult_ = result;
        }

        [[nodiscard]] std::size_t tickCalls() const noexcept
        {
            return tickCalls_;
        }

        [[nodiscard]] std::size_t lastTickBudget() const noexcept
        {
            return lastTickBudget_;
        }

        [[nodiscard]] const std::vector<ProcessId> &createdPids() const noexcept
        {
            return createdPids_;
        }

        [[nodiscard]] const std::vector<ProcessId> &terminatedPids() const noexcept
        {
            return terminatedPids_;
        }

        private:
        Result<void> createResult_ = Result<void>::ok();
        Result<void> dispatchResult_ = Result<void>::error(ErrorCode::NotFound);
        Result<void> terminateResult_ = Result<void>::ok();
        std::set<ProcessId> knownPids_;
        std::vector<ProcessId> createdPids_;
        std::vector<ProcessId> terminatedPids_;
        std::size_t tickCalls_ = 0;
        std::size_t lastTickBudget_ = 0;
    };

    class FakeDispatchRuntime final : public IDispatchRuntime
    {
        public:
        explicit FakeDispatchRuntime(HostThreadingConfig config = {})
            : config_(config.normalized())
        {}

        [[nodiscard]] std::string_view name() const noexcept override
        {
            return "FakeDispatchRuntime";
        }

        [[nodiscard]] const HostThreadingConfig &config() const noexcept override
        {
            return config_;
        }

        [[nodiscard]] Result<void> dispatch(const DispatcherLanes &lanes, std::size_t tickBudget) override
        {
            ++dispatchCalls_;
            lastLaneCount_ = lanes.size();
            lastTickBudget_ = tickBudget;
            return dispatchResult_;
        }

        void tick(const DispatcherLanes &lanes) override
        {
            ++tickCalls_;
            lastTickLaneCount_ = lanes.size();
        }

        void setDispatchResult(Result<void> result)
        {
            dispatchResult_ = result;
        }

        [[nodiscard]] std::size_t dispatchCalls() const noexcept
        {
            return dispatchCalls_;
        }

        [[nodiscard]] std::size_t tickCalls() const noexcept
        {
            return tickCalls_;
        }

        [[nodiscard]] std::size_t lastLaneCount() const noexcept
        {
            return lastLaneCount_;
        }

        [[nodiscard]] std::size_t lastTickLaneCount() const noexcept
        {
            return lastTickLaneCount_;
        }

        [[nodiscard]] std::size_t lastTickBudget() const noexcept
        {
            return lastTickBudget_;
        }

        private:
        HostThreadingConfig config_;
        Result<void> dispatchResult_ = Result<void>::ok();
        std::size_t dispatchCalls_ = 0;
        std::size_t tickCalls_ = 0;
        std::size_t lastLaneCount_ = 0;
        std::size_t lastTickLaneCount_ = 0;
        std::size_t lastTickBudget_ = 0;
    };

    std::unique_ptr<ProcessImage> makeProcess(ProcessId pid)
    {
        return std::make_unique<ProcessImage>(pid, "p" + std::to_string(pid), std::vector<Block>{});
    }

} // namespace

TEST(MPDispatcherTest, EmptyLanesPropagateInvalidStateWithConfiguredRuntime)
{
    SerialDispatchRuntime runtime;
    MPDispatcher mp({}, runtime);

    auto result = mp.dispatch(11);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidState);
}

TEST(MPDispatcherTest, CreateProcessRejectsNull)
{
    FakeDispatcher child;
    SerialDispatchRuntime runtime;
    MPDispatcher mp({std::ref(child)}, runtime);

    auto result = mp.createProcess(nullptr, 0);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidArgument);
}

TEST(MPDispatcherTest, CreateProcessReturnsInvalidStateWhenNoLanes)
{
    SerialDispatchRuntime runtime;
    MPDispatcher mp({}, runtime);

    auto result = mp.createProcess(makeProcess(5), 0);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidState);
}

TEST(MPDispatcherTest, CreateProcessRoutesByPidModulo)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    FakeDispatcher d2;
    SerialDispatchRuntime runtime;
    MPDispatcher mp({std::ref(d0), std::ref(d1), std::ref(d2)}, runtime);

    ASSERT_TRUE(mp.createProcess(makeProcess(1), 0).isOk()); // -> d1
    ASSERT_TRUE(mp.createProcess(makeProcess(2), 0).isOk()); // -> d2
    ASSERT_TRUE(mp.createProcess(makeProcess(3), 0).isOk()); // -> d0

    ASSERT_EQ(d0.createdPids().size(), 1u);
    ASSERT_EQ(d1.createdPids().size(), 1u);
    ASSERT_EQ(d2.createdPids().size(), 1u);
    EXPECT_EQ(d0.createdPids()[0], 3u);
    EXPECT_EQ(d1.createdPids()[0], 1u);
    EXPECT_EQ(d2.createdPids()[0], 2u);
}

TEST(MPDispatcherTest, DispatchReturnsOkWhenAnyChildSucceeds)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    FakeDispatcher d2;
    d0.setDispatchResult(Result<void>::error(ErrorCode::NotFound));
    d1.setDispatchResult(Result<void>::ok());
    d2.setDispatchResult(Result<void>::error(ErrorCode::InvalidState));
    SerialDispatchRuntime runtime;
    MPDispatcher mp({std::ref(d0), std::ref(d1), std::ref(d2)}, runtime);

    auto result = mp.dispatch(17);

    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(d0.lastTickBudget(), 17u);
    EXPECT_EQ(d1.lastTickBudget(), 17u);
    EXPECT_EQ(d2.lastTickBudget(), 17u);
}

TEST(MPDispatcherTest, DispatchReturnsFirstNonNotFoundErrorWhenNoSuccess)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    d0.setDispatchResult(Result<void>::error(ErrorCode::InvalidState));
    d1.setDispatchResult(Result<void>::error(ErrorCode::NotFound));
    SerialDispatchRuntime runtime;
    MPDispatcher mp({std::ref(d0), std::ref(d1)}, runtime);

    auto result = mp.dispatch(3);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidState);
}

TEST(MPDispatcherTest, DispatchReturnsNotFoundWhenAllChildrenNotFound)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    d0.setDispatchResult(Result<void>::error(ErrorCode::NotFound));
    d1.setDispatchResult(Result<void>::error(ErrorCode::NotFound));
    SerialDispatchRuntime runtime;
    MPDispatcher mp({std::ref(d0), std::ref(d1)}, runtime);

    auto result = mp.dispatch(5);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST(MPDispatcherTest, TickIsBroadcastToAllChildren)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    SerialDispatchRuntime runtime;
    MPDispatcher mp({std::ref(d0), std::ref(d1)}, runtime);

    mp.tick();
    mp.tick();

    EXPECT_EQ(d0.tickCalls(), 2u);
    EXPECT_EQ(d1.tickCalls(), 2u);
}

TEST(MPDispatcherTest, DispatchDelegatesToInjectedCustomRuntime)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    FakeDispatchRuntime runtime;
    runtime.setDispatchResult(Result<void>::error(ErrorCode::ResourceBusy));
    MPDispatcher mp({std::ref(d0), std::ref(d1)}, runtime);

    auto result = mp.dispatch(29);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::ResourceBusy);
    EXPECT_EQ(runtime.dispatchCalls(), 1U);
    EXPECT_EQ(runtime.lastLaneCount(), 2U);
    EXPECT_EQ(runtime.lastTickBudget(), 29U);
}

TEST(MPDispatcherTest, TickDelegatesToInjectedCustomRuntime)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    FakeDispatchRuntime runtime;
    MPDispatcher mp({std::ref(d0), std::ref(d1)}, runtime);

    mp.tick();
    mp.tick();

    EXPECT_EQ(runtime.tickCalls(), 2U);
    EXPECT_EQ(runtime.lastTickLaneCount(), 2U);
    EXPECT_EQ(d0.tickCalls(), 0U);
    EXPECT_EQ(d1.tickCalls(), 0U);
}

TEST(MPDispatcherTest, TerminateProcessRoutesToOwningChild)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    SerialDispatchRuntime runtime;
    MPDispatcher mp({std::ref(d0), std::ref(d1)}, runtime);

    ASSERT_TRUE(d1.createProcess(makeProcess(42), 0).isOk());

    auto result = mp.terminateProcess(42, 10);

    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(d1.terminatedPids().size(), 1u);
    EXPECT_EQ(d1.terminatedPids().front(), 42u);
    EXPECT_FALSE(d1.hasProcess(42));
}

TEST(MPDispatcherTest, TerminateProcessReturnsNotFoundForUnknownPid)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    SerialDispatchRuntime runtime;
    MPDispatcher mp({std::ref(d0), std::ref(d1)}, runtime);

    auto result = mp.terminateProcess(777, 1);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST(MPDispatcherTest, ProcessCountAndHasProcessAggregateChildren)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    SerialDispatchRuntime runtime;
    MPDispatcher mp({std::ref(d0), std::ref(d1)}, runtime);

    ASSERT_TRUE(d0.createProcess(makeProcess(10), 0).isOk());
    ASSERT_TRUE(d1.createProcess(makeProcess(11), 0).isOk());

    EXPECT_EQ(mp.processCount(), 2u);
    EXPECT_TRUE(mp.hasProcess(10));
    EXPECT_TRUE(mp.hasProcess(11));
    EXPECT_FALSE(mp.hasProcess(99));
}

TEST(DispatchRuntimeTest, FakeRuntimeExposesMetadataAndCallStats)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    DispatcherLanes lanes{std::ref(d0), std::ref(d1)};

    FakeDispatchRuntime runtime({.hostThreadCount = 0, .deterministicMode = false, .workStealingEnabled = true});
    runtime.setDispatchResult(Result<void>::error(ErrorCode::InvalidState));

    auto result = runtime.dispatch(lanes, 23);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidState);
    EXPECT_EQ(runtime.name(), "FakeDispatchRuntime");
    EXPECT_EQ(runtime.config().hostThreadCount, 1U);
    EXPECT_TRUE(runtime.config().isSingleThreaded());
    EXPECT_FALSE(runtime.config().workStealingEnabled);
    EXPECT_FALSE(runtime.config().deterministicMode);
    EXPECT_EQ(runtime.dispatchCalls(), 1U);
    EXPECT_EQ(runtime.lastLaneCount(), 2U);
    EXPECT_EQ(runtime.lastTickBudget(), 23U);

    runtime.tick(lanes);
    EXPECT_EQ(runtime.tickCalls(), 1U);
    EXPECT_EQ(runtime.lastTickLaneCount(), 2U);
}

TEST(DispatchRuntimeTest, SerialRuntimeNormalizesToSingleThreadedBaseline)
{
    SerialDispatchRuntime runtime({.hostThreadCount = 4, .deterministicMode = false, .workStealingEnabled = true});

    EXPECT_EQ(runtime.name(), "SerialDispatchRuntime");
    EXPECT_TRUE(runtime.config().isValid());
    EXPECT_TRUE(runtime.config().isSingleThreaded());
    EXPECT_EQ(runtime.config().hostThreadCount, 1U);
    EXPECT_FALSE(runtime.config().workStealingEnabled);
    EXPECT_FALSE(runtime.config().deterministicMode);
}

TEST(DispatchRuntimeTest, SerialRuntimeDispatchReturnsInvalidStateWhenNoLanes)
{
    SerialDispatchRuntime runtime;
    DispatcherLanes lanes;

    auto result = runtime.dispatch(lanes, 7);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidState);
}

TEST(DispatchRuntimeTest, SerialRuntimeDispatchAggregatesLaneResults)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    FakeDispatcher d2;
    d0.setDispatchResult(Result<void>::error(ErrorCode::NotFound));
    d1.setDispatchResult(Result<void>::ok());
    d2.setDispatchResult(Result<void>::error(ErrorCode::InvalidState));
    DispatcherLanes lanes{std::ref(d0), std::ref(d1), std::ref(d2)};
    SerialDispatchRuntime runtime;

    auto result = runtime.dispatch(lanes, 19);

    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(d0.lastTickBudget(), 19U);
    EXPECT_EQ(d1.lastTickBudget(), 19U);
    EXPECT_EQ(d2.lastTickBudget(), 19U);
}

TEST(DispatchRuntimeTest, SerialRuntimeDispatchReturnsFirstNonNotFoundError)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    d0.setDispatchResult(Result<void>::error(ErrorCode::InvalidArgument));
    d1.setDispatchResult(Result<void>::error(ErrorCode::NotFound));
    DispatcherLanes lanes{std::ref(d0), std::ref(d1)};
    SerialDispatchRuntime runtime;

    auto result = runtime.dispatch(lanes, 5);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidArgument);
}

TEST(DispatchRuntimeTest, SerialRuntimeTickBroadcastsToAllLanes)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    DispatcherLanes lanes{std::ref(d0), std::ref(d1)};
    SerialDispatchRuntime runtime;

    runtime.tick(lanes);
    runtime.tick(lanes);

    EXPECT_EQ(d0.tickCalls(), 2U);
    EXPECT_EQ(d1.tickCalls(), 2U);
}
