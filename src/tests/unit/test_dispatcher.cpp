/// @file test_dispatcher.cpp
/// @brief Unit tests for Dispatcher.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "contur/core/clock.h"
#include "contur/core/error.h"

#include "contur/arch/block.h"
#include "contur/arch/instruction.h"
#include "contur/dispatch/dispatcher.h"
#include "contur/execution/execution_context.h"
#include "contur/execution/i_execution_engine.h"
#include "contur/memory/fifo_replacement.h"
#include "contur/memory/mmu.h"
#include "contur/memory/physical_memory.h"
#include "contur/memory/virtual_memory.h"
#include "contur/process/process_image.h"
#include "contur/scheduling/fcfs_policy.h"
#include "contur/scheduling/i_scheduling_policy.h"
#include "contur/scheduling/scheduler.h"

using namespace contur;

namespace {

    class FakeExecutionEngine final : public IExecutionEngine
    {
        public:
        ExecutionResult execute(ProcessImage &process, std::size_t tickBudget) override
        {
            (void)tickBudget;
            lastExecutedPid_ = process.id();

            if (!scriptedResults_.empty())
            {
                ExecutionResult result = scriptedResults_.front();
                scriptedResults_.erase(scriptedResults_.begin());
                return result;
            }

            return defaultResult_;
        }

        void halt(ProcessId pid) override
        {
            lastHaltedPid_ = pid;
        }

        std::string_view name() const noexcept override
        {
            return "FakeExecutionEngine";
        }

        void setDefaultResult(ExecutionResult result)
        {
            defaultResult_ = result;
        }

        void pushResult(ExecutionResult result)
        {
            scriptedResults_.push_back(result);
        }

        [[nodiscard]] ProcessId lastExecutedPid() const noexcept
        {
            return lastExecutedPid_;
        }

        [[nodiscard]] ProcessId lastHaltedPid() const noexcept
        {
            return lastHaltedPid_;
        }

        private:
        ExecutionResult defaultResult_ = ExecutionResult::budgetExhausted(INVALID_PID, 0);
        std::vector<ExecutionResult> scriptedResults_;
        ProcessId lastExecutedPid_ = INVALID_PID;
        ProcessId lastHaltedPid_ = INVALID_PID;
    };

    class AlwaysUnknownPolicy final : public ISchedulingPolicy
    {
        public:
        std::string_view name() const noexcept override
        {
            return "AlwaysUnknownPolicy";
        }

        ProcessId
        selectNext(const std::vector<std::reference_wrapper<const PCB>> &readyQueue, const IClock &clock) const override
        {
            (void)readyQueue;
            (void)clock;
            return 999;
        }

        bool shouldPreempt(const PCB &running, const PCB &candidate, const IClock &clock) const override
        {
            (void)running;
            (void)candidate;
            (void)clock;
            return true;
        }
    };

    std::unique_ptr<ProcessImage> makeProcess(ProcessId pid, int value = 1)
    {
        std::vector<Block> code = {
            {Instruction::Mov, 0, value, 0},
            {Instruction::Halt, 0, 0, 0},
        };
        return std::make_unique<ProcessImage>(pid, "p" + std::to_string(pid), std::move(code));
    }

} // namespace

class DispatcherTest : public ::testing::Test
{
    protected:
    PhysicalMemory memory_{256};
    Mmu mmu_{memory_, std::make_unique<FifoReplacement>()};
    VirtualMemory virtualMemory_{mmu_, 16};
    SimulationClock clock_;
    Scheduler scheduler_{std::make_unique<FcfsPolicy>()};
    FakeExecutionEngine engine_;
    Dispatcher dispatcher_{scheduler_, engine_, virtualMemory_, clock_};
};

TEST_F(DispatcherTest, CreateProcessRejectsNull)
{
    auto result = dispatcher_.createProcess(nullptr, clock_.now());

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidArgument);
}

TEST_F(DispatcherTest, CreateProcessRejectsInvalidPid)
{
    for (ProcessId pid = 1; pid <= 16; ++pid)
    {
        ASSERT_TRUE(dispatcher_.createProcess(makeProcess(pid), clock_.now()).isOk());
    }

    auto result = dispatcher_.createProcess(makeProcess(17), clock_.now());

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::OutOfMemory);
}

TEST_F(DispatcherTest, CreateProcessLoadsMemoryAndEnqueues)
{
    auto result = dispatcher_.createProcess(makeProcess(1), clock_.now());

    ASSERT_TRUE(result.isOk());
    EXPECT_TRUE(dispatcher_.hasProcess(1));
    EXPECT_EQ(dispatcher_.processCount(), 1u);
    EXPECT_TRUE(virtualMemory_.hasSlot(1));
}

TEST_F(DispatcherTest, CreateProcessRejectsDuplicatePid)
{
    ASSERT_TRUE(dispatcher_.createProcess(makeProcess(1), clock_.now()).isOk());

    auto duplicate = dispatcher_.createProcess(makeProcess(1), clock_.now());

    EXPECT_TRUE(duplicate.isError());
    EXPECT_EQ(duplicate.errorCode(), ErrorCode::AlreadyExists);
}

TEST_F(DispatcherTest, DispatchWithoutProcessesFails)
{
    auto result = dispatcher_.dispatch(5);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST_F(DispatcherTest, DispatchBudgetExhaustedKeepsProcessAlive)
{
    ASSERT_TRUE(dispatcher_.createProcess(makeProcess(1), clock_.now()).isOk());
    engine_.setDefaultResult(ExecutionResult::budgetExhausted(1, 3));

    auto result = dispatcher_.dispatch(10);

    ASSERT_TRUE(result.isOk());
    EXPECT_TRUE(dispatcher_.hasProcess(1));
    EXPECT_TRUE(virtualMemory_.hasSlot(1));
    EXPECT_EQ(clock_.now(), 3u);
    EXPECT_EQ(engine_.lastExecutedPid(), 1u);
}

TEST_F(DispatcherTest, DispatchInterruptedMovesRunningToBlocked)
{
    ASSERT_TRUE(dispatcher_.createProcess(makeProcess(2), clock_.now()).isOk());
    engine_.setDefaultResult(ExecutionResult::interrupted(2, 1, Interrupt::SystemCall));

    auto result = dispatcher_.dispatch(5);

    ASSERT_TRUE(result.isOk());
    auto blocked = scheduler_.getBlockedSnapshot();
    ASSERT_EQ(blocked.size(), 1u);
    EXPECT_EQ(blocked.front(), 2u);
    EXPECT_EQ(scheduler_.runningProcess(), INVALID_PID);
    EXPECT_TRUE(dispatcher_.hasProcess(2));
}

TEST_F(DispatcherTest, DispatchProcessExitedRemovesProcessAndFreesMemory)
{
    ASSERT_TRUE(dispatcher_.createProcess(makeProcess(3), clock_.now()).isOk());
    engine_.setDefaultResult(ExecutionResult::exited(3, 2));

    auto result = dispatcher_.dispatch(8);

    ASSERT_TRUE(result.isOk());
    EXPECT_FALSE(dispatcher_.hasProcess(3));
    EXPECT_EQ(dispatcher_.processCount(), 0u);
    EXPECT_FALSE(virtualMemory_.hasSlot(3));
}

TEST_F(DispatcherTest, DispatchErrorRemovesProcess)
{
    ASSERT_TRUE(dispatcher_.createProcess(makeProcess(4), clock_.now()).isOk());
    engine_.setDefaultResult(ExecutionResult::error(4, 1, Interrupt::Error));

    auto result = dispatcher_.dispatch(8);

    ASSERT_TRUE(result.isOk());
    EXPECT_FALSE(dispatcher_.hasProcess(4));
    EXPECT_FALSE(virtualMemory_.hasSlot(4));
}

TEST_F(DispatcherTest, DispatchHaltedRemovesProcess)
{
    ASSERT_TRUE(dispatcher_.createProcess(makeProcess(5), clock_.now()).isOk());
    engine_.setDefaultResult(ExecutionResult::halted(5, 1));

    auto result = dispatcher_.dispatch(8);

    ASSERT_TRUE(result.isOk());
    EXPECT_FALSE(dispatcher_.hasProcess(5));
    EXPECT_FALSE(virtualMemory_.hasSlot(5));
}

TEST_F(DispatcherTest, DispatchFailsWhenSelectedProcessIsUnknown)
{
    ASSERT_TRUE(dispatcher_.createProcess(makeProcess(1), clock_.now()).isOk());
    ASSERT_TRUE(scheduler_.setPolicy(std::make_unique<AlwaysUnknownPolicy>()).isOk());

    auto result = dispatcher_.dispatch(1);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidState);
}

TEST_F(DispatcherTest, TickAdvancesClock)
{
    EXPECT_EQ(clock_.now(), 0u);

    dispatcher_.tick();

    EXPECT_EQ(clock_.now(), 1u);
}

TEST_F(DispatcherTest, TerminateProcessRemovesManagedProcess)
{
    ASSERT_TRUE(dispatcher_.createProcess(makeProcess(6), clock_.now()).isOk());

    auto result = dispatcher_.terminateProcess(6, clock_.now());

    ASSERT_TRUE(result.isOk()) << static_cast<int>(result.errorCode());
    EXPECT_FALSE(dispatcher_.hasProcess(6));
    EXPECT_FALSE(virtualMemory_.hasSlot(6));
    EXPECT_EQ(dispatcher_.processCount(), 0u);
}

TEST_F(DispatcherTest, TerminateProcessRejectsUnknownPid)
{
    auto result = dispatcher_.terminateProcess(77, clock_.now());

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}
