/// @file test_kernel_end_to_end.cpp
/// @brief End-to-end integration tests for the full kernel pipeline.
///
/// Tests exercise the complete path: KernelBuilder → process creation →
/// dispatcher → scheduler → InterpreterEngine → process termination, and
/// verify all observable outcomes (tick counter, process count, snapshot
/// fields, syscall dispatch, virtual memory lifecycle).

#include <algorithm>
#include <memory>
#include <span>
#include <vector>

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
#include "contur/process/process_image.h"
#include "contur/scheduling/fcfs_policy.h"
#include "contur/scheduling/priority_policy.h"
#include "contur/scheduling/round_robin_policy.h"
#include "contur/scheduling/scheduler.h"
#include "contur/syscall/syscall_ids.h"
#include "contur/syscall/syscall_table.h"
#include "contur/sync/mutex.h"
#include "contur/tracing/null_tracer.h"

using namespace contur;

namespace {

    Result<std::unique_ptr<IKernel>> buildKernel(
        std::unique_ptr<ISchedulingPolicy> policy = nullptr, std::size_t tickBudget = 4
    )
    {
        auto clock = std::make_unique<SimulationClock>();
        auto tracer = std::make_unique<NullTracer>(*clock);
        auto memory = std::make_unique<PhysicalMemory>(512);
        auto mmu = std::make_unique<Mmu>(*memory, std::make_unique<FifoReplacement>(), *tracer);
        auto virtualMem = std::make_unique<VirtualMemory>(*mmu, MAX_PROCESSES);
        auto cpu = std::make_unique<Cpu>(*memory);
        auto engine = std::make_unique<InterpreterEngine>(*cpu, *memory);
        if (!policy)
            policy = std::make_unique<RoundRobinPolicy>(tickBudget);
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

    /// @brief Build a process that executes @p nops NOPs then HALTs.
    ProcessConfig makeNopProcess(
        const char *name, std::size_t nops = 8, PriorityLevel pri = PriorityLevel::Normal
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

    /// @brief Drain the kernel until all processes finish or @p maxTicks reached.
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
// Basic process lifecycle
// ---------------------------------------------------------------------------

TEST(KernelE2ETest, SingleProcessRunsToCompletion)
{
    auto buildResult = buildKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ASSERT_TRUE(kernel->createProcess(makeNopProcess("svc", 4)).isOk());
    EXPECT_EQ(kernel->processCount(), 1u);

    drainKernel(*kernel);

    EXPECT_EQ(kernel->processCount(), 0u);
}

TEST(KernelE2ETest, MultipleProcessesAllCompleteEventually)
{
    auto buildResult = buildKernel(std::make_unique<RoundRobinPolicy>(2));
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ASSERT_TRUE(kernel->createProcess(makeNopProcess("p1", 8)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNopProcess("p2", 12)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNopProcess("p3", 6)).isOk());
    EXPECT_EQ(kernel->processCount(), 3u);

    drainKernel(*kernel);

    EXPECT_EQ(kernel->processCount(), 0u);
}

TEST(KernelE2ETest, ProcessWithArithmeticInstructionsCompletesCleanly)
{
    auto buildResult = buildKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ProcessConfig cfg;
    cfg.name = "arith";
    cfg.code = {
        {Instruction::Mov, 0, 5, 0},
        {Instruction::Mov, 1, 3, 0},
        {Instruction::Add, 2, 0, 1},
        {Instruction::Halt, 0, 0, 0},
    };
    ASSERT_TRUE(kernel->createProcess(cfg).isOk());

    drainKernel(*kernel);
    EXPECT_EQ(kernel->processCount(), 0u);
}

// ---------------------------------------------------------------------------
// Tick counter
// ---------------------------------------------------------------------------

TEST(KernelE2ETest, TickCounterAdvancesWithEachDispatch)
{
    auto buildResult = buildKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ASSERT_TRUE(kernel->createProcess(makeNopProcess("ticker", 32)).isOk());

    Tick t0 = kernel->now();
    ASSERT_TRUE(kernel->tick(1).isOk());
    Tick t1 = kernel->now();
    ASSERT_TRUE(kernel->tick(1).isOk());
    Tick t2 = kernel->now();

    EXPECT_GT(t1, t0);
    EXPECT_GT(t2, t1);
}

TEST(KernelE2ETest, SnapshotTickAdvancesAfterTick)
{
    auto buildResult = buildKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ASSERT_TRUE(kernel->createProcess(makeNopProcess("tick-snap", 32)).isOk());

    Tick before = kernel->snapshot().currentTick;
    ASSERT_TRUE(kernel->tick(1).isOk());
    Tick after = kernel->snapshot().currentTick;

    EXPECT_GT(after, before);
}

// ---------------------------------------------------------------------------
// Snapshot accuracy
// ---------------------------------------------------------------------------

TEST(KernelE2ETest, SnapshotProcessCountMatchesLiveProcesses)
{
    auto buildResult = buildKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ASSERT_TRUE(kernel->createProcess(makeNopProcess("sp1", 32)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNopProcess("sp2", 32)).isOk());

    auto snap = kernel->snapshot();
    EXPECT_EQ(snap.processCount, 2u);
    EXPECT_GE(snap.readyCount, 1u);
}

TEST(KernelE2ETest, SnapshotListsProcessNamesCorrectly)
{
    auto buildResult = buildKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ASSERT_TRUE(kernel->createProcess(makeNopProcess("alpha", 32)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNopProcess("beta", 32)).isOk());

    auto snap = kernel->snapshot();
    ASSERT_EQ(snap.processes.size(), 2u);

    auto hasName = [&](const std::string &name) {
        return std::any_of(snap.processes.begin(), snap.processes.end(), [&](const KernelProcessSnapshot &p) {
            return p.name == name;
        });
    };
    EXPECT_TRUE(hasName("alpha"));
    EXPECT_TRUE(hasName("beta"));
}

TEST(KernelE2ETest, SnapshotPolicyNameReflectsScheduler)
{
    auto buildResult = buildKernel(std::make_unique<RoundRobinPolicy>(4));
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ASSERT_TRUE(kernel->createProcess(makeNopProcess("polname", 8)).isOk());
    EXPECT_EQ(kernel->snapshot().policyName, "RoundRobin");
}

// ---------------------------------------------------------------------------
// Scheduling policy integration
// ---------------------------------------------------------------------------

TEST(KernelE2ETest, FcfsTwoProcessesBothComplete)
{
    auto buildResult = buildKernel(std::make_unique<FcfsPolicy>(), 2);
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ASSERT_TRUE(kernel->createProcess(makeNopProcess("fcfs-a", 4)).isOk());
    ASSERT_TRUE(kernel->createProcess(makeNopProcess("fcfs-b", 4)).isOk());

    drainKernel(*kernel);
    EXPECT_EQ(kernel->processCount(), 0u);
}

TEST(KernelE2ETest, PriorityPolicyRealtimeFinishesBeforeIdleProcess)
{
    // Realtime has only 2 NOPs + HALT, Idle has 100 NOPs + HALT.
    // With priority scheduling and budget=1 per tick, Realtime should run
    // exclusively until it exits, after which Idle gets CPU.
    auto buildResult = buildKernel(std::make_unique<PriorityPolicy>(), 1);
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    auto rtPid = kernel->createProcess(makeNopProcess("rt", 2, PriorityLevel::Realtime)).value();
    ASSERT_TRUE(kernel->createProcess(makeNopProcess("idle-bg", 100, PriorityLevel::Idle)).isOk());

    // 5 ticks is more than enough for the 3-instruction realtime program
    ASSERT_TRUE(kernel->runForTicks(5, 1).isOk());

    EXPECT_FALSE(kernel->hasProcess(rtPid));
    // Idle process (100 NOPs) still alive — it had at most 2 CPU ticks
    EXPECT_GT(kernel->processCount(), 0u);
}

// ---------------------------------------------------------------------------
// Syscall dispatch
// ---------------------------------------------------------------------------

TEST(KernelE2ETest, RegisteredSyscallHandlerIsCallable)
{
    auto buildResult = buildKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    int calls = 0;
    ASSERT_TRUE(
        kernel
            ->registerSyscallHandler(
                SyscallId::GetTime,
                [&calls](std::span<const RegisterValue>, ProcessImage &) {
                    ++calls;
                    return Result<RegisterValue>::ok(123);
                }
            )
            .isOk()
    );

    auto pid = kernel->createProcess(makeNopProcess("sc-test")).value();
    auto result = kernel->syscall(pid, SyscallId::GetTime, {});

    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value(), 123);
    EXPECT_EQ(calls, 1);
}

TEST(KernelE2ETest, SyscallFailsForUnknownProcess)
{
    auto buildResult = buildKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ASSERT_TRUE(
        kernel
            ->registerSyscallHandler(
                SyscallId::GetPid,
                [](std::span<const RegisterValue>, ProcessImage &p) {
                    return Result<RegisterValue>::ok(static_cast<RegisterValue>(p.id()));
                }
            )
            .isOk()
    );

    auto result = kernel->syscall(9999, SyscallId::GetPid, {});
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST(KernelE2ETest, SyscallHandlerArgsForwardedCorrectly)
{
    auto buildResult = buildKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    std::vector<RegisterValue> received;
    ASSERT_TRUE(
        kernel
            ->registerSyscallHandler(
                SyscallId::Write,
                [&received](std::span<const RegisterValue> args, ProcessImage &) {
                    received = std::vector<RegisterValue>(args.begin(), args.end());
                    return Result<RegisterValue>::ok(static_cast<RegisterValue>(args.size()));
                }
            )
            .isOk()
    );

    auto pid = kernel->createProcess(makeNopProcess("arg-fwd")).value();
    const std::vector<RegisterValue> args = {10, 20, 30};
    auto result = kernel->syscall(pid, SyscallId::Write, args);

    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(received, args);
}

// ---------------------------------------------------------------------------
// Memory management
// ---------------------------------------------------------------------------

TEST(KernelE2ETest, VirtualMemorySlotsFreedAfterProcessTerminates)
{
    auto buildResult = buildKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    std::size_t freesBefore = kernel->snapshot().freeVirtualSlots;

    ASSERT_TRUE(kernel->createProcess(makeNopProcess("mem-test", 4)).isOk());
    EXPECT_LT(kernel->snapshot().freeVirtualSlots, freesBefore);

    drainKernel(*kernel);
    EXPECT_EQ(kernel->processCount(), 0u);

    // Slots should be returned after process finishes
    EXPECT_GE(kernel->snapshot().freeVirtualSlots, freesBefore);
}

TEST(KernelE2ETest, ManualTerminateDecreasesProcessCount)
{
    auto buildResult = buildKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    auto pid = kernel->createProcess(makeNopProcess("term-test", 32)).value();
    EXPECT_EQ(kernel->processCount(), 1u);

    ASSERT_TRUE(kernel->terminateProcess(pid).isOk());
    EXPECT_EQ(kernel->processCount(), 0u);
    EXPECT_FALSE(kernel->hasProcess(pid));
}

// ---------------------------------------------------------------------------
// runForTicks semantics (regression for Bug 2 anyWorkDone fix)
// ---------------------------------------------------------------------------

TEST(KernelE2ETest, RunForTicksReturnsNotFoundOnEmptyKernel)
{
    auto buildResult = buildKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    auto result = kernel->runForTicks(10, 1);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST(KernelE2ETest, RunForTicksReturnsOkWhenProcessFinishesMidCycle)
{
    auto buildResult = buildKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ASSERT_TRUE(kernel->createProcess(makeNopProcess("short", 4)).isOk());

    // Request far more cycles than needed — should return ok, not NotFound
    auto result = kernel->runForTicks(200, 1);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(kernel->processCount(), 0u);
}

TEST(KernelE2ETest, SyncPrimitiveEnterAndLeaveAroundProcess)
{
    auto buildResult = buildKernel();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ASSERT_TRUE(kernel->registerSyncPrimitive("mtx", std::make_unique<Mutex>()).isOk());

    auto pid = kernel->createProcess(makeNopProcess("sync-test")).value();
    EXPECT_TRUE(kernel->enterCritical(pid, "mtx").isOk());
    EXPECT_TRUE(kernel->leaveCritical(pid, "mtx").isOk());
}
