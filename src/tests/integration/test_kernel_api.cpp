/// @file test_kernel_api.cpp
/// @brief Integration tests for Kernel API wiring.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "contur/core/clock.h"

#include "contur/arch/instruction.h"
#include "contur/execution/execution_context.h"
#include "contur/execution/i_execution_engine.h"
#include "contur/kernel/i_kernel.h"
#include "contur/kernel/kernel_builder.h"
#include "contur/memory/fifo_replacement.h"
#include "contur/memory/physical_memory.h"
#include "contur/process/process_image.h"
#include "contur/scheduling/fcfs_policy.h"

using namespace contur;

namespace {

    class ScriptedExecutionEngine final : public IExecutionEngine
    {
        public:
        ExecutionResult execute(ProcessImage &process, std::size_t tickBudget) override
        {
            (void)tickBudget;
            lastExecutedPid_ = process.id();

            if (!scripted_.empty())
            {
                ExecutionResult out = scripted_.front();
                scripted_.erase(scripted_.begin());
                return out;
            }

            return ExecutionResult::budgetExhausted(process.id(), 1);
        }

        void halt(ProcessId pid) override
        {
            haltedPids_.push_back(pid);
        }

        std::string_view name() const noexcept override
        {
            return "ScriptedExecutionEngine";
        }

        void push(ExecutionResult result)
        {
            scripted_.push_back(result);
        }

        [[nodiscard]] ProcessId lastExecutedPid() const noexcept
        {
            return lastExecutedPid_;
        }

        [[nodiscard]] const std::vector<ProcessId> &haltedPids() const noexcept
        {
            return haltedPids_;
        }

        private:
        std::vector<ExecutionResult> scripted_;
        std::vector<ProcessId> haltedPids_;
        ProcessId lastExecutedPid_ = INVALID_PID;
    };

    ProcessConfig makeProgram(const char *name)
    {
        ProcessConfig cfg;
        cfg.name = name;
        cfg.code = {
            {Instruction::Mov, 0, 1, 0},
            {Instruction::Add, 0, 1, 0},
            {Instruction::Halt, 0, 0, 0},
        };
        return cfg;
    }

} // namespace

TEST(KernelApiIntegrationTest, BuilderInjectedExecutionEngineControlsLifecycle)
{
    auto engine = std::make_unique<ScriptedExecutionEngine>();
    auto *engineRaw = engine.get();

    auto kernel = KernelBuilder()
                      .withMemory(std::make_unique<PhysicalMemory>(256))
                      .withPageReplacementPolicy(std::make_unique<FifoReplacement>())
                      .withSchedulingPolicy(std::make_unique<FcfsPolicy>())
                      .withExecutionEngine(std::move(engine))
                      .withDefaultTickBudget(2)
                      .build();

    auto pid = kernel->createProcess(makeProgram("scripted"));
    ASSERT_TRUE(pid.isOk());

    engineRaw->push(ExecutionResult::budgetExhausted(pid.value(), 1));
    engineRaw->push(ExecutionResult::exited(pid.value(), 1));

    auto first = kernel->tick(1);
    ASSERT_TRUE(first.isOk());
    EXPECT_TRUE(kernel->hasProcess(pid.value()));
    EXPECT_EQ(engineRaw->lastExecutedPid(), pid.value());

    auto second = kernel->tick(1);
    ASSERT_TRUE(second.isOk());
    EXPECT_FALSE(kernel->hasProcess(pid.value()));
}

TEST(KernelApiIntegrationTest, KernelTerminateCallsEngineHalt)
{
    auto engine = std::make_unique<ScriptedExecutionEngine>();
    auto *engineRaw = engine.get();

    auto kernel = KernelBuilder()
                      .withMemory(std::make_unique<PhysicalMemory>(256))
                      .withPageReplacementPolicy(std::make_unique<FifoReplacement>())
                      .withExecutionEngine(std::move(engine))
                      .build();

    auto pid = kernel->createProcess(makeProgram("terminate"));
    ASSERT_TRUE(pid.isOk());
    ASSERT_TRUE(kernel->terminateProcess(pid.value()).isOk());

    ASSERT_EQ(engineRaw->haltedPids().size(), 1u);
    EXPECT_EQ(engineRaw->haltedPids().front(), pid.value());
}

TEST(KernelApiIntegrationTest, SyscallFailsAfterProcessTermination)
{
    auto kernel = KernelBuilder().build();
    auto pid = kernel->createProcess(makeProgram("syscall-end"));
    ASSERT_TRUE(pid.isOk());

    ASSERT_TRUE(kernel
                    ->registerSyscallHandler(
                        SyscallId::GetTime,
                        [](std::span<const RegisterValue>, ProcessImage &) { return Result<RegisterValue>::ok(123); }
                    )
                    .isOk());

    ASSERT_TRUE(kernel->terminateProcess(pid.value()).isOk());

    auto result = kernel->syscall(pid.value(), SyscallId::GetTime, {});

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}
