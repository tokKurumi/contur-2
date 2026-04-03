/// @file test_tui_tick_navigation.cpp
/// @brief Integration tests for TUI tick playback and history navigation semantics.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "contur/core/clock.h"

#include "contur/arch/instruction.h"
#include "contur/cpu/cpu.h"
#include "contur/dispatch/dispatcher.h"
#include "contur/execution/interpreter_engine.h"
#include "contur/fs/simple_fs.h"
#include "contur/ipc/ipc_manager.h"
#include "contur/kernel/i_kernel.h"
#include "contur/kernel/kernel_builder.h"
#include "contur/kernel/kernel_diagnostics.h"
#include "contur/memory/fifo_replacement.h"
#include "contur/memory/mmu.h"
#include "contur/memory/physical_memory.h"
#include "contur/memory/virtual_memory.h"
#include "contur/scheduling/round_robin_policy.h"
#include "contur/scheduling/scheduler.h"
#include "contur/syscall/syscall_table.h"
#include "contur/tracing/null_tracer.h"
#include "contur/tui/i_kernel_read_model.h"
#include "contur/tui/i_tui_controller.h"

using namespace contur;

namespace {

    Result<std::unique_ptr<IKernel>> buildKernelForTuiIntegration()
    {
        auto clock = std::make_unique<SimulationClock>();
        auto tracer = std::make_unique<NullTracer>(*clock);
        auto memory = std::make_unique<PhysicalMemory>(1024);
        auto mmu = std::make_unique<Mmu>(*memory, std::make_unique<FifoReplacement>(), *tracer);
        auto virtualMemory = std::make_unique<VirtualMemory>(*mmu, MAX_PROCESSES);
        auto cpu = std::make_unique<Cpu>(*memory);
        auto engine = std::make_unique<InterpreterEngine>(*cpu, *memory);
        auto scheduler = std::make_unique<Scheduler>(std::make_unique<RoundRobinPolicy>(1), *tracer);
        auto dispatcher = std::make_unique<Dispatcher>(*scheduler, *engine, *virtualMemory, *clock, *tracer);

        return KernelBuilder()
            .withClock(std::move(clock))
            .withMemory(std::move(memory))
            .withMmu(std::move(mmu))
            .withVirtualMemory(std::move(virtualMemory))
            .withCpu(std::move(cpu))
            .withExecutionEngine(std::move(engine))
            .withScheduler(std::move(scheduler))
            .withDispatcher(std::move(dispatcher))
            .withTracer(std::move(tracer))
            .withFileSystem(std::make_unique<SimpleFS>())
            .withIpcManager(std::make_unique<IpcManager>())
            .withSyscallTable(std::make_unique<SyscallTable>())
            .withDefaultTickBudget(1)
            .build();
    }

    ProcessConfig makeLongProgram(const char *name, std::size_t nopCount)
    {
        ProcessConfig config;
        config.name = name;
        config.code.reserve(nopCount + 1);
        for (std::size_t i = 0; i < nopCount; ++i)
        {
            config.code.push_back(Block{Instruction::Nop, 0, 0, 0});
        }
        config.code.push_back(Block{Instruction::Halt, 0, 0, 0});
        return config;
    }

} // namespace

TEST(TuiTickNavigationIntegrationTest, SeekMovesUiCursorWithoutKernelRollback)
{
    auto buildResult = buildKernelForTuiIntegration();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    auto created = kernel->createProcess(makeLongProgram("tui-history", 200));
    ASSERT_TRUE(created.isOk());

    KernelDiagnostics diagnostics(*kernel);
    KernelReadModel readModel(diagnostics);
    TuiController controller(
        readModel,
        [&kernel](std::size_t step) { return kernel->runForTicks(step, 1); },
        64
    );

    TuiCommand tick;
    tick.kind = TuiCommandKind::Tick;
    tick.step = 3;
    ASSERT_TRUE(controller.dispatch(tick).isOk());

    Tick afterThree = controller.current().currentTick;
    ASSERT_GE(afterThree, 3u);

    tick.step = 2;
    ASSERT_TRUE(controller.dispatch(tick).isOk());

    Tick latestUiTick = controller.current().currentTick;
    Tick latestKernelTick = kernel->now();
    ASSERT_GE(latestUiTick, afterThree);

    TuiCommand back;
    back.kind = TuiCommandKind::SeekBackward;
    back.step = 1;
    ASSERT_TRUE(controller.dispatch(back).isOk());

    Tick rewoundUiTick = controller.current().currentTick;
    EXPECT_LT(rewoundUiTick, latestUiTick);
    EXPECT_EQ(kernel->now(), latestKernelTick);

    TuiCommand forward;
    forward.kind = TuiCommandKind::SeekForward;
    forward.step = 1;
    ASSERT_TRUE(controller.dispatch(forward).isOk());

    EXPECT_EQ(controller.current().currentTick, latestUiTick);
}

TEST(TuiTickNavigationIntegrationTest, AutoplayStartPauseAndStopFollowContracts)
{
    auto buildResult = buildKernelForTuiIntegration();
    ASSERT_TRUE(buildResult.isOk());
    auto kernel = std::move(buildResult).value();

    auto created = kernel->createProcess(makeLongProgram("tui-autoplay", 200));
    ASSERT_TRUE(created.isOk());

    KernelDiagnostics diagnostics(*kernel);
    KernelReadModel readModel(diagnostics);
    TuiController controller(
        readModel,
        [&kernel](std::size_t step) { return kernel->runForTicks(step, 1); },
        64
    );

    TuiCommand start;
    start.kind = TuiCommandKind::AutoPlayStart;
    start.step = 2;
    start.intervalMs = 10;
    ASSERT_TRUE(controller.dispatch(start).isOk());
    ASSERT_EQ(controller.state(), TuiControllerState::Playing);

    Tick initialTick = controller.current().currentTick;

    ASSERT_TRUE(controller.advanceAutoplay(9).isOk());
    EXPECT_EQ(controller.current().currentTick, initialTick);

    ASSERT_TRUE(controller.advanceAutoplay(1).isOk());
    Tick afterFirstWindow = controller.current().currentTick;
    EXPECT_GT(afterFirstWindow, initialTick);

    ASSERT_TRUE(controller.advanceAutoplay(20).isOk());
    Tick afterThreeWindows = controller.current().currentTick;
    EXPECT_GT(afterThreeWindows, afterFirstWindow);

    TuiCommand pause;
    pause.kind = TuiCommandKind::Pause;
    ASSERT_TRUE(controller.dispatch(pause).isOk());
    ASSERT_EQ(controller.state(), TuiControllerState::Paused);

    ASSERT_TRUE(controller.advanceAutoplay(50).isOk());
    EXPECT_EQ(controller.current().currentTick, afterThreeWindows);

    TuiCommand stop;
    stop.kind = TuiCommandKind::AutoPlayStop;
    ASSERT_TRUE(controller.dispatch(stop).isOk());
    EXPECT_EQ(controller.state(), TuiControllerState::Idle);
}
