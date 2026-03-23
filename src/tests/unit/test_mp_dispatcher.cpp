/// @file test_mp_dispatcher.cpp
/// @brief Unit tests for MPDispatcher.

#include <functional>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "contur/dispatch/i_dispatcher.h"
#include "contur/dispatch/mp_dispatcher.h"
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

    std::unique_ptr<ProcessImage> makeProcess(ProcessId pid)
    {
        return std::make_unique<ProcessImage>(pid, "p" + std::to_string(pid), std::vector<Block>{});
    }

} // namespace

TEST(MPDispatcherTest, ConstructorRequiresAtLeastOneDispatcher)
{
    EXPECT_THROW((MPDispatcher(std::vector<std::reference_wrapper<IDispatcher>>{})), std::invalid_argument);
}

TEST(MPDispatcherTest, CreateProcessRejectsNull)
{
    FakeDispatcher child;
    MPDispatcher mp({std::ref(child)});

    auto result = mp.createProcess(nullptr, 0);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidArgument);
}

TEST(MPDispatcherTest, CreateProcessRoutesByPidModulo)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    FakeDispatcher d2;
    MPDispatcher mp({std::ref(d0), std::ref(d1), std::ref(d2)});

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
    MPDispatcher mp({std::ref(d0), std::ref(d1), std::ref(d2)});

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
    MPDispatcher mp({std::ref(d0), std::ref(d1)});

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
    MPDispatcher mp({std::ref(d0), std::ref(d1)});

    auto result = mp.dispatch(5);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST(MPDispatcherTest, TickIsBroadcastToAllChildren)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    MPDispatcher mp({std::ref(d0), std::ref(d1)});

    mp.tick();
    mp.tick();

    EXPECT_EQ(d0.tickCalls(), 2u);
    EXPECT_EQ(d1.tickCalls(), 2u);
}

TEST(MPDispatcherTest, TerminateProcessRoutesToOwningChild)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    MPDispatcher mp({std::ref(d0), std::ref(d1)});

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
    MPDispatcher mp({std::ref(d0), std::ref(d1)});

    auto result = mp.terminateProcess(777, 1);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST(MPDispatcherTest, ProcessCountAndHasProcessAggregateChildren)
{
    FakeDispatcher d0;
    FakeDispatcher d1;
    MPDispatcher mp({std::ref(d0), std::ref(d1)});

    ASSERT_TRUE(d0.createProcess(makeProcess(10), 0).isOk());
    ASSERT_TRUE(d1.createProcess(makeProcess(11), 0).isOk());

    EXPECT_EQ(mp.processCount(), 2u);
    EXPECT_TRUE(mp.hasProcess(10));
    EXPECT_TRUE(mp.hasProcess(11));
    EXPECT_FALSE(mp.hasProcess(99));
}
