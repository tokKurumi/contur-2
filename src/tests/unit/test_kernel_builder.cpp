/// @file test_kernel_builder.cpp
/// @brief Unit tests for Kernel and KernelBuilder.

#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "contur/core/clock.h"
#include "contur/core/error.h"

#include "contur/arch/instruction.h"
#include "contur/cpu/cpu.h"
#include "contur/dispatch/dispatcher.h"
#include "contur/dispatch/i_dispatcher.h"
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
#include "contur/scheduling/round_robin_policy.h"
#include "contur/scheduling/scheduler.h"
#include "contur/sync/mutex.h"
#include "contur/syscall/syscall_table.h"

using namespace contur;

namespace {

    template <typename Snapshot>
    concept HasHostThreadCountField = requires(Snapshot snapshot) { snapshot.hostThreadCount; };

    template <typename Snapshot>
    concept HasDeterministicModeField = requires(Snapshot snapshot) { snapshot.deterministicMode; };

    template <typename Snapshot>
    concept HasWorkStealingEnabledField = requires(Snapshot snapshot) { snapshot.workStealingEnabled; };

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

            ProcessId pid = process->id();
            if (processes_.find(pid) != processes_.end())
            {
                return Result<void>::error(ErrorCode::AlreadyExists);
            }

            processes_.emplace(pid, std::move(process));
            ++createCalls_;
            return createResult_;
        }

        Result<void> dispatch(std::size_t tickBudget) override
        {
            lastBudget_ = tickBudget;
            ++dispatchCalls_;
            return dispatchResult_;
        }

        Result<void> terminateProcess(ProcessId pid, Tick currentTick) override
        {
            (void)currentTick;
            auto it = processes_.find(pid);
            if (it == processes_.end())
            {
                return Result<void>::error(ErrorCode::NotFound);
            }
            processes_.erase(it);
            ++terminateCalls_;
            return Result<void>::ok();
        }

        void tick() override
        {}

        std::size_t processCount() const noexcept override
        {
            return processes_.size();
        }

        bool hasProcess(ProcessId pid) const noexcept override
        {
            return processes_.find(pid) != processes_.end();
        }

        void setDispatchResult(Result<void> result)
        {
            dispatchResult_ = result;
        }

        [[nodiscard]] std::size_t createCalls() const noexcept
        {
            return createCalls_;
        }

        [[nodiscard]] std::size_t dispatchCalls() const noexcept
        {
            return dispatchCalls_;
        }

        [[nodiscard]] std::size_t terminateCalls() const noexcept
        {
            return terminateCalls_;
        }

        [[nodiscard]] std::size_t lastBudget() const noexcept
        {
            return lastBudget_;
        }

        private:
        std::unordered_map<ProcessId, std::unique_ptr<ProcessImage>> processes_;
        Result<void> createResult_ = Result<void>::ok();
        Result<void> dispatchResult_ = Result<void>::ok();
        std::size_t createCalls_ = 0;
        std::size_t dispatchCalls_ = 0;
        std::size_t terminateCalls_ = 0;
        std::size_t lastBudget_ = 0;
    };

    KernelBuilder makeStrictBuilder(
        std::unique_ptr<IDispatcher> dispatcher = nullptr,
        std::size_t defaultTickBudget = static_cast<std::size_t>(DEFAULT_TIME_SLICE)
    )
    {
        auto clock = std::make_unique<SimulationClock>();
        auto memory = std::make_unique<PhysicalMemory>(2048);
        auto mmu = std::make_unique<Mmu>(*memory, std::make_unique<FifoReplacement>());
        auto virtualMemory = std::make_unique<VirtualMemory>(*mmu, MAX_PROCESSES);
        auto cpu = std::make_unique<Cpu>(*memory);
        auto engine = std::make_unique<InterpreterEngine>(*cpu, *memory);
        auto scheduler = std::make_unique<Scheduler>(std::make_unique<RoundRobinPolicy>(defaultTickBudget));

        if (!dispatcher)
        {
            dispatcher = std::make_unique<Dispatcher>(*scheduler, *engine, *virtualMemory, *clock);
        }

        KernelBuilder builder;
        [[maybe_unused]] auto &configured = builder.withClock(std::move(clock))
                                                .withMemory(std::move(memory))
                                                .withMmu(std::move(mmu))
                                                .withVirtualMemory(std::move(virtualMemory))
                                                .withCpu(std::move(cpu))
                                                .withExecutionEngine(std::move(engine))
                                                .withScheduler(std::move(scheduler))
                                                .withDispatcher(std::move(dispatcher))
                                                .withFileSystem(std::make_unique<SimpleFS>())
                                                .withIpcManager(std::make_unique<IpcManager>())
                                                .withSyscallTable(std::make_unique<SyscallTable>())
                                                .withDefaultTickBudget(defaultTickBudget);
        return builder;
    }

    ProcessConfig makeConfig(const char *name = "p")
    {
        ProcessConfig cfg;
        cfg.name = name;
        cfg.code = {
            {Instruction::Mov, 0, 1, 0},
            {Instruction::Halt, 0, 0, 0},
        };
        return cfg;
    }

} // namespace

TEST(KernelBuilderTest, BuildFailsWhenDependenciesAreMissing)
{
    auto buildResult = KernelBuilder().build();

    ASSERT_TRUE(buildResult.isError());
    EXPECT_EQ(buildResult.errorCode(), ErrorCode::InvalidState);
}

TEST(KernelBuilderTest, SnapshotContractRemainsRuntimeAgnostic)
{
    static_assert(!HasHostThreadCountField<KernelSnapshot>);
    static_assert(!HasDeterministicModeField<KernelSnapshot>);
    static_assert(!HasWorkStealingEnabledField<KernelSnapshot>);

    SUCCEED();
}

TEST(KernelBuilderTest, BuildKernelWithExplicitDependencies)
{
    auto buildResult = makeStrictBuilder().build();

    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ASSERT_NE(kernel, nullptr);
    EXPECT_EQ(kernel->processCount(), 0u);
    EXPECT_EQ(kernel->now(), 0u);

    KernelSnapshot s = kernel->snapshot();
    EXPECT_EQ(s.processCount, 0u);
    EXPECT_TRUE(s.runningPids.empty());
}

TEST(KernelBuilderTest, CreateProcessAutoAssignsPid)
{
    auto buildResult = makeStrictBuilder().build();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    auto p1 = kernel->createProcess(makeConfig("p1"));
    auto p2 = kernel->createProcess(makeConfig("p2"));

    ASSERT_TRUE(p1.isOk());
    ASSERT_TRUE(p2.isOk());
    EXPECT_NE(p1.value(), INVALID_PID);
    EXPECT_NE(p2.value(), INVALID_PID);
    EXPECT_NE(p1.value(), p2.value());
    EXPECT_TRUE(kernel->hasProcess(p1.value()));
    EXPECT_TRUE(kernel->hasProcess(p2.value()));
}

TEST(KernelBuilderTest, CreateProcessRejectsEmptyProgram)
{
    auto buildResult = makeStrictBuilder().build();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ProcessConfig cfg;
    cfg.name = "bad";

    auto result = kernel->createProcess(cfg);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidArgument);
}

TEST(KernelBuilderTest, TickUsesDefaultBudgetWhenZero)
{
    auto fakeDispatcher = std::make_unique<FakeDispatcher>();
    FakeDispatcher *dispatcherRaw = fakeDispatcher.get();

    auto buildResult = makeStrictBuilder(std::move(fakeDispatcher), 7).build();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    ASSERT_TRUE(kernel->createProcess(makeConfig()).isOk());

    auto tickResult = kernel->tick(0);

    ASSERT_TRUE(tickResult.isOk());
    EXPECT_EQ(dispatcherRaw->dispatchCalls(), 1u);
    EXPECT_EQ(dispatcherRaw->lastBudget(), 7u);
}

TEST(KernelBuilderTest, BuildFailsWhenDefaultTickBudgetIsZero)
{
    auto buildResult = makeStrictBuilder(nullptr, 0).build();

    ASSERT_TRUE(buildResult.isError());
    EXPECT_EQ(buildResult.errorCode(), ErrorCode::InvalidArgument);
}

TEST(KernelBuilderTest, TerminateProcessDelegatesToDispatcher)
{
    auto fakeDispatcher = std::make_unique<FakeDispatcher>();
    FakeDispatcher *dispatcherRaw = fakeDispatcher.get();

    auto buildResult = makeStrictBuilder(std::move(fakeDispatcher)).build();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    auto pid = kernel->createProcess(makeConfig());
    ASSERT_TRUE(pid.isOk());

    auto result = kernel->terminateProcess(pid.value());

    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(dispatcherRaw->terminateCalls(), 1u);
    EXPECT_FALSE(kernel->hasProcess(pid.value()));
}

TEST(KernelBuilderTest, SyscallRoundTripWithCallerContext)
{
    auto buildResult = makeStrictBuilder().build();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    auto pid = kernel->createProcess(makeConfig("syscaller"));
    ASSERT_TRUE(pid.isOk());

    ASSERT_TRUE(kernel
                    ->registerSyscallHandler(
                        SyscallId::GetPid,
                        [](std::span<const RegisterValue>, ProcessImage &caller) {
                            return Result<RegisterValue>::ok(static_cast<RegisterValue>(caller.id()));
                        }
                    )
                    .isOk());

    auto syscallResult = kernel->syscall(pid.value(), SyscallId::GetPid, {});

    ASSERT_TRUE(syscallResult.isOk());
    EXPECT_EQ(syscallResult.value(), static_cast<RegisterValue>(pid.value()));
}

TEST(KernelBuilderTest, RunForTicksStopsGracefullyWhenNoProcesses)
{
    auto fakeDispatcher = std::make_unique<FakeDispatcher>();
    fakeDispatcher->setDispatchResult(Result<void>::error(ErrorCode::NotFound));

    auto buildResult = makeStrictBuilder(std::move(fakeDispatcher)).build();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    auto result = kernel->runForTicks(20, 3);

    EXPECT_TRUE(result.isOk());
}

TEST(KernelBuilderTest, SyncPrimitiveLifecycle)
{
    auto buildResult = makeStrictBuilder().build();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    auto pid = kernel->createProcess(makeConfig("lock-owner"));
    ASSERT_TRUE(pid.isOk());

    ASSERT_TRUE(kernel->registerSyncPrimitive("mutex.main", std::make_unique<Mutex>()).isOk());

    auto lock = kernel->enterCritical(pid.value(), "mutex.main");
    auto unlock = kernel->leaveCritical(pid.value(), "mutex.main");

    EXPECT_TRUE(lock.isOk());
    EXPECT_TRUE(unlock.isOk());
}

TEST(KernelBuilderTest, SnapshotTracksReadyQueueAfterCreate)
{
    auto buildResult = makeStrictBuilder().build();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    auto pid = kernel->createProcess(makeConfig("snapshot-p"));
    ASSERT_TRUE(pid.isOk());

    KernelSnapshot s = kernel->snapshot();

    EXPECT_EQ(s.processCount, 1u);
    EXPECT_EQ(s.readyCount, 1u);
    EXPECT_TRUE(s.runningPids.empty());
    EXPECT_GT(s.totalVirtualSlots, 0u);
}
