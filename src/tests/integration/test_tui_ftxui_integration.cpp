/// @file test_tui_ftxui_integration.cpp
/// @brief Integration tests: kernel → diagnostics → read-model → controller → FtxuiRenderer.
///
/// These tests exercise the full TUI stack end-to-end without a real terminal.
/// They build a minimal kernel, spawn processes, tick the simulation, and verify
/// that the FTXUI renderer produces expected output.

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "contur/core/clock.h"
#include "contur/core/types.h"

#include "contur/arch/instruction.h"
#include "contur/cpu/cpu.h"
#include "contur/dispatch/dispatcher.h"
#include "contur/execution/interpreter_engine.h"
#include "contur/fs/simple_fs.h"
#include "contur/io/device_manager.h"
#include "contur/io/io_manager.h"
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
#include "contur/tui/ftxui_renderer.h"
#include "contur/tui/i_kernel_read_model.h"
#include "contur/tui/i_tui_controller.h"
#include "contur/tui/tui_commands.h"

using namespace contur;

class FtxuiIntegrationTest : public ::testing::Test
{
    protected:
    void SetUp() override
    {
        auto clock = std::make_unique<SimulationClock>();
        auto tracer = std::make_unique<NullTracer>(*clock);
        auto memory = std::make_unique<PhysicalMemory>(1024);
        auto replacement = std::make_unique<FifoReplacement>();
        auto mmu = std::make_unique<Mmu>(*memory, std::move(replacement), *tracer);
        auto virtualMem = std::make_unique<VirtualMemory>(*mmu, MAX_PROCESSES);
        auto cpu = std::make_unique<Cpu>(*memory);
        auto engine = std::make_unique<InterpreterEngine>(*cpu, *memory);
        auto policy = std::make_unique<RoundRobinPolicy>(4);
        auto scheduler = std::make_unique<Scheduler>(std::move(policy), *tracer);
        auto syscallTable = std::make_unique<SyscallTable>();
        auto dispatcher =
            std::make_unique<Dispatcher>(*scheduler, *engine, *virtualMem, *clock, *tracer, *syscallTable);
        auto fs = std::make_unique<SimpleFS>(64);
        auto deviceManager = std::make_unique<DeviceManager>();
        auto ioManager = std::make_unique<IoManager>(*fs, *deviceManager);
        auto ipc = std::make_unique<IpcManager>();

        auto result = KernelBuilder{}
                          .withClock(std::move(clock))
                          .withMemory(std::move(memory))
                          .withMmu(std::move(mmu))
                          .withVirtualMemory(std::move(virtualMem))
                          .withCpu(std::move(cpu))
                          .withExecutionEngine(std::move(engine))
                          .withScheduler(std::move(scheduler))
                          .withDispatcher(std::move(dispatcher))
                          .withTracer(std::move(tracer))
                          .withFileSystem(std::move(fs))
                          .withDeviceManager(std::move(deviceManager))
                          .withIoManager(std::move(ioManager))
                          .withIpcManager(std::move(ipc))
                          .withSyscallTable(std::move(syscallTable))
                          .withDefaultTickBudget(4)
                          .build();

        ASSERT_TRUE(result.isOk()) << "Kernel build failed";
        kernel_ = std::move(result).value();

        diagnostics_ = std::make_unique<KernelDiagnostics>(*kernel_);
        readModel_ = std::make_unique<KernelReadModel>(*diagnostics_);

        controller_ = std::make_unique<TuiController>(
            *readModel_, [this](std::size_t step) { return kernel_->runForTicks(step); }, 128
        );

        renderer_ = std::make_unique<FtxuiRenderer>(160, 50);
    }

    /// @brief Spawn a process with enough NOPs to run for several ticks.
    void spawnProcess(
        const char *name, PriorityLevel pri = PriorityLevel::Normal, std::int32_t nice = 0, std::size_t nops = 32
    )
    {
        ProcessConfig cfg;
        cfg.name = name;
        cfg.priority = Priority{pri, pri, nice};
        cfg.code.reserve(nops + 1);
        for (std::size_t i = 0; i < nops; ++i)
        {
            cfg.code.push_back(Block{Instruction::Nop, 0, 0, 0});
        }
        cfg.code.push_back(Block{Instruction::Halt, 0, 0, 0});

        auto r = kernel_->createProcess(cfg);
        ASSERT_TRUE(r.isOk()) << "Failed to create process '" << name << "'";
    }

    std::unique_ptr<IKernel> kernel_;
    std::unique_ptr<KernelDiagnostics> diagnostics_;
    std::unique_ptr<KernelReadModel> readModel_;
    std::unique_ptr<TuiController> controller_;
    std::unique_ptr<FtxuiRenderer> renderer_;
};

// Stack wiring

TEST_F(FtxuiIntegrationTest, ControllerStartsWithInitialSnapshot)
{
    EXPECT_GE(controller_->historySize(), std::size_t{1});
}

TEST_F(FtxuiIntegrationTest, RenderInitialSnapshotOk)
{
    const auto &snap = controller_->current();
    ASSERT_TRUE(renderer_->render(snap).isOk());
    EXPECT_TRUE(renderer_->hasContent());
}

TEST_F(FtxuiIntegrationTest, RenderedOutputContainsSimulatorTitle)
{
    ASSERT_TRUE(renderer_->render(controller_->current()).isOk());
    EXPECT_NE(renderer_->lastRendered().find("CONTUR2"), std::string::npos);
}

TEST_F(FtxuiIntegrationTest, SpawnedProcessesAppearInRenderedOutput)
{
    spawnProcess("testsvc", PriorityLevel::High);
    spawnProcess("bgworker", PriorityLevel::Low, 10);

    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Tick, 1, 0}).isOk());
    ASSERT_TRUE(renderer_->render(controller_->current()).isOk());

    const auto &out = renderer_->lastRendered();
    EXPECT_NE(out.find("testsvc"), std::string::npos);
    EXPECT_NE(out.find("bgworker"), std::string::npos);
}

TEST_F(FtxuiIntegrationTest, ProcessCountReflectedAfterSpawn)
{
    spawnProcess("a");
    spawnProcess("b");
    spawnProcess("c");

    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Tick, 1, 0}).isOk());
    EXPECT_EQ(controller_->current().processCount, std::size_t{3});
}

TEST_F(FtxuiIntegrationTest, TickAdvancesSimulationTick)
{
    spawnProcess("ticker");
    Tick before = controller_->current().currentTick;
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Tick, 1, 0}).isOk());
    EXPECT_GT(controller_->current().currentTick, before);
}

TEST_F(FtxuiIntegrationTest, MultipleTicksAccumulateHistory)
{
    spawnProcess("accumulator");
    for (int i = 0; i < 5; ++i)
    {
        ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Tick, 1, 0}).isOk());
    }
    EXPECT_GE(controller_->historySize(), std::size_t{6}); // initial + 5 ticks
}

TEST_F(FtxuiIntegrationTest, SeekBackwardRestoresPastSnapshot)
{
    spawnProcess("p1");

    // Each dispatch creates one history entry; dispatch 3 times to get 4 entries
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Tick, 1, 0}).isOk());
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Tick, 1, 0}).isOk());
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Tick, 1, 0}).isOk());
    Tick tickAtLatest = controller_->current().currentTick;

    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::SeekBackward, 1, 0}).isOk());
    Tick tickAtPrev = controller_->current().currentTick;

    EXPECT_LE(tickAtPrev, tickAtLatest) << "Seeking backward should show an earlier or equal tick";
}

TEST_F(FtxuiIntegrationTest, SeekForwardAfterBackwardReturnsToLatest)
{
    spawnProcess("seeker");
    // Each dispatch creates one history entry; dispatch 3 times → 4 entries total
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Tick, 1, 0}).isOk());
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Tick, 1, 0}).isOk());
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Tick, 1, 0}).isOk());
    Tick latest = controller_->current().currentTick;

    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::SeekBackward, 2, 0}).isOk());
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::SeekForward, 2, 0}).isOk());

    EXPECT_EQ(controller_->current().currentTick, latest);
}

TEST_F(FtxuiIntegrationTest, SeekBeyondHistoryReturnsError)
{
    // Only the initial snapshot exists
    auto r = controller_->dispatch(TuiCommand{TuiCommandKind::SeekBackward, 999, 0});
    EXPECT_TRUE(r.isError());
}

TEST_F(FtxuiIntegrationTest, AutoplayStartSetsPlayingState)
{
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::AutoPlayStart, 1, 100}).isOk());
    EXPECT_EQ(controller_->state(), TuiControllerState::Playing);
}

TEST_F(FtxuiIntegrationTest, PauseSwitchesToPausedState)
{
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::AutoPlayStart, 1, 100}).isOk());
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Pause, 1, 0}).isOk());
    EXPECT_EQ(controller_->state(), TuiControllerState::Paused);
}

TEST_F(FtxuiIntegrationTest, AutoplayStopReturnsToIdle)
{
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::AutoPlayStart, 1, 100}).isOk());
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::AutoPlayStop, 1, 0}).isOk());
    EXPECT_EQ(controller_->state(), TuiControllerState::Idle);
}

TEST_F(FtxuiIntegrationTest, AdvanceAutoplayFiresTicksWhenPlaying)
{
    spawnProcess("autoworker");

    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::AutoPlayStart, 1, 100}).isOk());

    Tick before = controller_->current().currentTick;
    // Advance by 250ms — should fire 2 ticks at 100ms interval
    ASSERT_TRUE(controller_->advanceAutoplay(250).isOk());
    Tick after = controller_->current().currentTick;

    EXPECT_GT(after, before);
}

TEST_F(FtxuiIntegrationTest, AdvanceAutoplayDoesNothingWhenPaused)
{
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::AutoPlayStart, 1, 100}).isOk());
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Pause, 1, 0}).isOk());

    Tick before = controller_->current().currentTick;
    ASSERT_TRUE(controller_->advanceAutoplay(500).isOk());
    Tick after = controller_->current().currentTick;

    EXPECT_EQ(before, after);
}

TEST_F(FtxuiIntegrationTest, TickNumberAppearsInRenderedOutput)
{
    spawnProcess("app");
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Tick, 5, 0}).isOk());

    Tick tick = controller_->current().currentTick;
    ASSERT_TRUE(renderer_->render(controller_->current()).isOk());
    EXPECT_NE(renderer_->lastRendered().find(std::to_string(tick)), std::string::npos);
}

TEST_F(FtxuiIntegrationTest, HistoryCursorReflectedInSeek)
{
    spawnProcess("cursor-test");
    // Dispatch 3 times to have history to seek back into
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Tick, 1, 0}).isOk());
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Tick, 1, 0}).isOk());
    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::Tick, 1, 0}).isOk());
    std::size_t cursorBefore = controller_->historyCursor();

    ASSERT_TRUE(controller_->dispatch(TuiCommand{TuiCommandKind::SeekBackward, 1, 0}).isOk());
    EXPECT_LT(controller_->historyCursor(), cursorBefore);
}
