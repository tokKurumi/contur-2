/// @file test_process_scheduling_integration.cpp
/// @brief Integration tests verifying correct process scheduling behaviour
///        across FCFS, RoundRobin, and Priority scheduling policies.

#include <memory>

#include <gtest/gtest.h>

#include "contur/core/clock.h"
#include "contur/core/error.h"

#include "contur/arch/block.h"
#include "contur/arch/instruction.h"
#include "contur/cpu/cpu.h"
#include "contur/dispatch/dispatcher.h"
#include "contur/execution/interpreter_engine.h"
#include "contur/fs/simple_fs.h"
#include "contur/ipc/ipc_manager.h"
#include "contur/kernel/i_kernel.h"
#include "contur/kernel/kernel_builder.h"
#include "contur/memory/fifo_replacement.h"
#include "contur/memory/mmu.h"
#include "contur/memory/physical_memory.h"
#include "contur/memory/virtual_memory.h"
#include "contur/scheduling/fcfs_policy.h"
#include "contur/scheduling/priority_policy.h"
#include "contur/scheduling/round_robin_policy.h"
#include "contur/scheduling/scheduler.h"
#include "contur/syscall/syscall_table.h"
#include "contur/tracing/null_tracer.h"

using namespace contur;

namespace {

    Result<std::unique_ptr<IKernel>> buildKernelWithPolicy(
        std::unique_ptr<ISchedulingPolicy> policy, std::size_t tickBudget = 2
    )
    {
        auto clock = std::make_unique<SimulationClock>();
        auto tracer = std::make_unique<NullTracer>(*clock);
        auto memory = std::make_unique<PhysicalMemory>(512);
        auto mmu = std::make_unique<Mmu>(*memory, std::make_unique<FifoReplacement>(), *tracer);
        auto virtualMem = std::make_unique<VirtualMemory>(*mmu, MAX_PROCESSES);
        auto cpu = std::make_unique<Cpu>(*memory);
        auto engine = std::make_unique<InterpreterEngine>(*cpu, *memory);
        auto scheduler = std::make_unique<Scheduler>(std::move(policy), *tracer);
        auto dispatcher = std::make_unique<Dispatcher>(*scheduler, *engine, *virtualMem, *clock, *tracer);

        return KernelBuilder{}
            .withClock(std::move(clock))
            .withMemory(std::move(memory))
            .withMmu(std::move(mmu))
            .withVirtualMemory(std::move(virtualMem))
            .withCpu(std::move(cpu))
            .withExecutionEngine(std::move(engine))
            .withScheduler(std::move(scheduler))
            .withDispatcher(std::move(dispatcher))
            .withTracer(std::move(tracer))
            .withFileSystem(std::make_unique<SimpleFS>())
            .withIpcManager(std::make_unique<IpcManager>())
            .withSyscallTable(std::make_unique<SyscallTable>())
            .withDefaultTickBudget(tickBudget)
            .build();
    }

    ProcessConfig makeNops(
        const char *name, std::size_t nops, PriorityLevel pri = PriorityLevel::Normal
    )
    {
        ProcessConfig cfg;
        cfg.name = name;
        cfg.priority = Priority{pri, pri, 0};
        cfg.code.reserve(nops + 1);
        for (std::size_t i = 0; i < nops; ++i)
            cfg.code.push_back({Instruction::Nop, 0, 0, 0});
        cfg.code.push_back({Instruction::Halt, 0, 0, 0});
        return cfg;
    }

    void drainKernel(IKernel &kernel, std::size_t maxTicks = 500)
    {
        for (std::size_t i = 0; i < maxTicks; ++i)
        {
            if (kernel.processCount() == 0)
                break;
            auto r = kernel.tick(1);
            if (r.isError() && r.errorCode() == ErrorCode::NotFound)
                break;
        }
    }

} // namespace

// ---------------------------------------------------------------------------
// Round Robin
// ---------------------------------------------------------------------------

TEST(SchedulingIntTest, RoundRobinThreeProcessesAllComplete)
{
    auto res = buildKernelWithPolicy(std::make_unique<RoundRobinPolicy>(1), 1);
    ASSERT_TRUE(res.isOk());
    auto kernel = std::move(res).value();

    ASSERT_TRUE(kernel->createProcess(makeNops("rr-a", 4)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNops("rr-b", 4)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNops("rr-c", 4)).isOk());

    drainKernel(*kernel);
    EXPECT_EQ(kernel->processCount(), 0u);
}

TEST(SchedulingIntTest, RoundRobinAllProcessesStillLiveAfterOneTick)
{
    // Time slice = 1, 3 long-running processes — after 1 tick all should still exist
    auto res = buildKernelWithPolicy(std::make_unique<RoundRobinPolicy>(1), 1);
    ASSERT_TRUE(res.isOk());
    auto kernel = std::move(res).value();

    ASSERT_TRUE(kernel->createProcess(makeNops("rr-long-a", 32)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNops("rr-long-b", 32)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNops("rr-long-c", 32)).isOk());

    ASSERT_TRUE(kernel->tick(1).isOk());
    EXPECT_EQ(kernel->processCount(), 3u);
}

TEST(SchedulingIntTest, RoundRobinLargeBatchCompletes)
{
    auto res = buildKernelWithPolicy(std::make_unique<RoundRobinPolicy>(2), 2);
    ASSERT_TRUE(res.isOk());
    auto kernel = std::move(res).value();

    for (int i = 0; i < 8; ++i)
    {
        std::string name = "batch-" + std::to_string(i);
        ASSERT_TRUE(kernel->createProcess(makeNops(name.c_str(), 6)).isOk());
    }

    drainKernel(*kernel);
    EXPECT_EQ(kernel->processCount(), 0u);
}

// ---------------------------------------------------------------------------
// FCFS
// ---------------------------------------------------------------------------

TEST(SchedulingIntTest, FcfsThreeProcessesAllComplete)
{
    auto res = buildKernelWithPolicy(std::make_unique<FcfsPolicy>(), 4);
    ASSERT_TRUE(res.isOk());
    auto kernel = std::move(res).value();

    ASSERT_TRUE(kernel->createProcess(makeNops("fcfs-1", 8)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNops("fcfs-2", 8)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNops("fcfs-3", 8)).isOk());

    drainKernel(*kernel);
    EXPECT_EQ(kernel->processCount(), 0u);
}

TEST(SchedulingIntTest, FcfsSingleProcessCompletes)
{
    auto res = buildKernelWithPolicy(std::make_unique<FcfsPolicy>(), 2);
    ASSERT_TRUE(res.isOk());
    auto kernel = std::move(res).value();

    ASSERT_TRUE(kernel->createProcess(makeNops("fcfs-solo", 10)).isOk());
    drainKernel(*kernel);
    EXPECT_EQ(kernel->processCount(), 0u);
}

// ---------------------------------------------------------------------------
// Priority
// ---------------------------------------------------------------------------

TEST(SchedulingIntTest, PriorityAllPrioritiesCompleteEventually)
{
    auto res = buildKernelWithPolicy(std::make_unique<PriorityPolicy>(), 1);
    ASSERT_TRUE(res.isOk());
    auto kernel = std::move(res).value();

    ASSERT_TRUE(kernel->createProcess(makeNops("idle", 4, PriorityLevel::Idle)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNops("low", 4, PriorityLevel::Low)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNops("normal", 4, PriorityLevel::Normal)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNops("high", 4, PriorityLevel::High)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNops("realtime", 4, PriorityLevel::Realtime)).isOk());

    drainKernel(*kernel);
    EXPECT_EQ(kernel->processCount(), 0u);
}

TEST(SchedulingIntTest, PriorityRealtimeCompletesBeforeIdleIsHalfDone)
{
    // Realtime: 2 NOPs + HALT = 3 instructions (3 ticks with budget=1)
    // Idle: 100 NOPs + HALT — will not have run until Realtime is done
    auto res = buildKernelWithPolicy(std::make_unique<PriorityPolicy>(), 1);
    ASSERT_TRUE(res.isOk());
    auto kernel = std::move(res).value();

    auto rtPid = kernel->createProcess(makeNops("rt", 2, PriorityLevel::Realtime)).value();
    ASSERT_TRUE(kernel->createProcess(makeNops("bg", 100, PriorityLevel::Idle)).isOk());

    // Run just enough ticks for Realtime to finish
    ASSERT_TRUE(kernel->runForTicks(5, 1).isOk());

    // Realtime done; Idle (100 NOPs) barely started
    EXPECT_FALSE(kernel->hasProcess(rtPid));
    EXPECT_GT(kernel->processCount(), 0u);
}

// ---------------------------------------------------------------------------
// Mixed: process creation / termination interleaved with scheduling
// ---------------------------------------------------------------------------

TEST(SchedulingIntTest, CreateProcessDuringRuntime)
{
    auto res = buildKernelWithPolicy(std::make_unique<RoundRobinPolicy>(2), 2);
    ASSERT_TRUE(res.isOk());
    auto kernel = std::move(res).value();

    ASSERT_TRUE(kernel->createProcess(makeNops("initial", 32)).isOk());

    ASSERT_TRUE(kernel->tick(1).isOk());
    ASSERT_TRUE(kernel->tick(1).isOk());

    // Late-arriving process joins mid-simulation
    ASSERT_TRUE(kernel->createProcess(makeNops("late", 32)).isOk());
    EXPECT_EQ(kernel->processCount(), 2u);

    drainKernel(*kernel);
    EXPECT_EQ(kernel->processCount(), 0u);
}

TEST(SchedulingIntTest, ManualTerminateOneOfThreeProcesses)
{
    auto res = buildKernelWithPolicy(std::make_unique<RoundRobinPolicy>(2), 2);
    ASSERT_TRUE(res.isOk());
    auto kernel = std::move(res).value();

    auto pid1 = kernel->createProcess(makeNops("p1", 32)).value();
    ASSERT_TRUE(kernel->createProcess(makeNops("p2", 32)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNops("p3", 32)).isOk());

    ASSERT_TRUE(kernel->tick(1).isOk()); // one dispatch cycle
    ASSERT_TRUE(kernel->terminateProcess(pid1).isOk());
    EXPECT_FALSE(kernel->hasProcess(pid1));
    EXPECT_GE(kernel->processCount(), 1u);

    drainKernel(*kernel);
    EXPECT_EQ(kernel->processCount(), 0u);
}

TEST(SchedulingIntTest, KernelNowMonotonicallyIncreasesDuringRun)
{
    auto res = buildKernelWithPolicy(std::make_unique<RoundRobinPolicy>(1), 1);
    ASSERT_TRUE(res.isOk());
    auto kernel = std::move(res).value();

    ASSERT_TRUE(kernel->createProcess(makeNops("mono", 16)).isOk());

    Tick prev = kernel->now();
    for (int i = 0; i < 8; ++i)
    {
        if (kernel->processCount() == 0)
            break;
        ASSERT_TRUE(kernel->tick(1).isOk());
        Tick cur = kernel->now();
        EXPECT_GE(cur, prev);
        prev = cur;
    }
}
