/// @file test_kernel_api.cpp
/// @brief Integration tests for Kernel API wiring.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "contur/core/clock.h"
#include "contur/arch/instruction.h"
#include "contur/cpu/cpu.h"
#include "contur/dispatch/dispatcher.h"
#include "contur/dispatch/i_dispatcher.h"
#include "contur/execution/execution_context.h"
#include "contur/execution/i_execution_engine.h"
#include "contur/execution/interpreter_engine.h"
#include "contur/fs/simple_fs.h"
#include "contur/ipc/ipc_manager.h"
#include "contur/kernel/i_kernel.h"
#include "contur/kernel/kernel_builder.h"
#include "contur/memory/fifo_replacement.h"
#include "contur/memory/mmu.h"
#include "contur/memory/physical_memory.h"
#include "contur/memory/virtual_memory.h"
#include "contur/process/process_image.h"
#include "contur/scheduling/fcfs_policy.h"
#include "contur/scheduling/round_robin_policy.h"
#include "contur/scheduling/scheduler.h"
#include "contur/syscall/syscall_table.h"
#include "contur/tracing/null_tracer.h"

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

    class ErrorDispatcher final : public IDispatcher
    {
        public:
        explicit ErrorDispatcher(ErrorCode dispatchError)
            : dispatchError_(dispatchError)
        {}

        [[nodiscard]] Result<void> createProcess(std::unique_ptr<ProcessImage> process, Tick currentTick) override
        {
            (void)currentTick;
            if (!process)
            {
                return Result<void>::error(ErrorCode::InvalidArgument);
            }
            processes_.push_back(std::move(process));
            return Result<void>::ok();
        }

        [[nodiscard]] Result<void> dispatch(std::size_t tickBudget) override
        {
            (void)tickBudget;
            return Result<void>::error(dispatchError_);
        }

        [[nodiscard]] Result<void> terminateProcess(ProcessId pid, Tick currentTick) override
        {
            (void)currentTick;
            for (auto it = processes_.begin(); it != processes_.end(); ++it)
            {
                if ((*it)->id() == pid)
                {
                    processes_.erase(it);
                    return Result<void>::ok();
                }
            }
            return Result<void>::error(ErrorCode::NotFound);
        }

        void tick() override
        {}

        [[nodiscard]] std::size_t processCount() const noexcept override
        {
            return processes_.size();
        }

        [[nodiscard]] bool hasProcess(ProcessId pid) const noexcept override
        {
            for (const auto &process : processes_)
            {
                if (process->id() == pid)
                {
                    return true;
                }
            }
            return false;
        }

        private:
        std::vector<std::unique_ptr<ProcessImage>> processes_;
        ErrorCode dispatchError_;
    };

    Result<std::unique_ptr<IKernel>> buildStrictKernel(
        std::unique_ptr<IExecutionEngine> executionEngine = nullptr,
        std::unique_ptr<ISchedulingPolicy> schedulingPolicy = nullptr,
        std::unique_ptr<IDispatcher> dispatcher = nullptr,
        std::size_t defaultTickBudget = 2
    )
    {
        auto clock = std::make_unique<SimulationClock>();
        auto tracer = std::make_unique<NullTracer>(*clock);
        auto memory = std::make_unique<PhysicalMemory>(256);
        auto mmu = std::make_unique<Mmu>(*memory, std::make_unique<FifoReplacement>(), *tracer);
        auto virtualMemory = std::make_unique<VirtualMemory>(*mmu, MAX_PROCESSES);
        auto cpu = std::make_unique<Cpu>(*memory);

        if (!executionEngine)
        {
            executionEngine = std::make_unique<InterpreterEngine>(*cpu, *memory);
        }

        if (!schedulingPolicy)
        {
            schedulingPolicy = std::make_unique<RoundRobinPolicy>(defaultTickBudget);
        }

        auto scheduler = std::make_unique<Scheduler>(std::move(schedulingPolicy), *tracer);
        if (!dispatcher)
        {
            dispatcher = std::make_unique<Dispatcher>(*scheduler, *executionEngine, *virtualMemory, *clock, *tracer);
        }

        return KernelBuilder()
            .withClock(std::move(clock))
            .withMemory(std::move(memory))
            .withMmu(std::move(mmu))
            .withVirtualMemory(std::move(virtualMemory))
            .withCpu(std::move(cpu))
            .withExecutionEngine(std::move(executionEngine))
            .withScheduler(std::move(scheduler))
            .withDispatcher(std::move(dispatcher))
            .withTracer(std::move(tracer))
            .withFileSystem(std::make_unique<SimpleFS>())
            .withIpcManager(std::make_unique<IpcManager>())
            .withSyscallTable(std::make_unique<SyscallTable>())
            .withDefaultTickBudget(defaultTickBudget)
            .build();
    }

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

    auto buildResult = buildStrictKernel(std::move(engine), std::make_unique<FcfsPolicy>(), nullptr, 2);
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

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

    auto buildResult = buildStrictKernel(std::move(engine));
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    auto pid = kernel->createProcess(makeProgram("terminate"));
    ASSERT_TRUE(pid.isOk());
    ASSERT_TRUE(kernel->terminateProcess(pid.value()).isOk());

    ASSERT_EQ(engineRaw->haltedPids().size(), 1u);
    EXPECT_EQ(engineRaw->haltedPids().front(), pid.value());
}

TEST(KernelApiIntegrationTest, SyscallFailsAfterProcessTermination)
{
    auto buildResult = buildStrictKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

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

TEST(KernelApiIntegrationTest, DispatcherErrorPropagatesThroughKernelTick)
{
    auto buildResult =
        buildStrictKernel(nullptr, nullptr, std::make_unique<ErrorDispatcher>(ErrorCode::ResourceBusy), 2);
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    auto pid = kernel->createProcess(makeProgram("runtime-error"));
    ASSERT_TRUE(pid.isOk());

    auto tickResult = kernel->tick(1);

    EXPECT_TRUE(tickResult.isError());
    EXPECT_EQ(tickResult.errorCode(), ErrorCode::ResourceBusy);
}
