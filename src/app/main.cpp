/// @file main.cpp
/// @brief Contur 2 — OS Kernel Simulator interactive TUI entry point.
///
/// Builds a demo kernel with several processes and launches the FTXUI-backed
/// interactive visualiser. The simulation starts paused; press [Space] to
/// begin autoplay or [t] to advance one tick manually.

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "contur/core/clock.h"

#include "contur/arch/block.h"
#include "contur/arch/instruction.h"
#include "contur/arch/interrupt.h"
#include "contur/arch/register_file.h"
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
#include "contur/tracing/buffer_sink.h"
#include "contur/tracing/trace_level.h"
#include "contur/tracing/trace_sink.h"
#include "contur/tracing/tracer.h"
#include "contur/tui/ftxui_app.h"
#include "contur/tui/i_kernel_read_model.h"
#include "contur/tui/i_tui_controller.h"

using namespace contur;

namespace {

    class SharedBufferTraceSink final : public ITraceSink
    {
        public:
        explicit SharedBufferTraceSink(std::shared_ptr<BufferSink> sink)
            : sink_(std::move(sink))
        {}

        void write(const TraceEvent &event) override
        {
            sink_->write(event);
        }

        private:
        std::shared_ptr<BufferSink> sink_;
    };

    struct DemoKernelBuild
    {
        Result<std::unique_ptr<IKernel>> kernelResult;
        std::shared_ptr<BufferSink> traceSink;
    };

} // namespace

static std::vector<Block> makeProgramAddOnePlusOne()
{
    return {
        {Instruction::Mov, 0, 1, 0},
        {Instruction::Add, 0, 1, 0},
        {Instruction::Halt, 0, 0, 0},
    };
}

static std::vector<Block> makeProgramCounterLoop()
{
    return {
        {Instruction::Mov, 0, 0, 0},
        {Instruction::Add, 0, 1, 0},
        {Instruction::Compare, 0, 16, 0},
        {Instruction::JumpLess, 0, 1, 0},
        {Instruction::Halt, 0, 0, 0},
    };
}

static std::vector<Block> makeProgramCpuHeavy()
{
    return {
        {Instruction::Mov, 1, 2, 0},
        {Instruction::Mov, 2, 3, 0},
        {Instruction::Mul, 1, 2, 1},
        {Instruction::Add, 1, 7, 0},
        {Instruction::Sub, 1, 1, 0},
        {Instruction::Compare, 1, 20, 0},
        {Instruction::JumpLess, 0, 2, 0},
        {Instruction::Halt, 0, 0, 0},
    };
}

static std::vector<Block> makeProgramLongNop(std::size_t nops)
{
    std::vector<Block> code;
    code.reserve(nops + 1);
    for (std::size_t i = 0; i < nops; ++i)
    {
        code.push_back({Instruction::Nop, 0, 0, 0});
    }
    code.push_back({Instruction::Halt, 0, 0, 0});
    return code;
}

static std::string formatTraceEventLine(const TraceEvent &event)
{
    std::string line = "[" + std::to_string(event.timestamp) + "]";
    line += "[" + std::string(traceLevelToString(event.level)) + "] ";
    line += event.subsystem + "." + event.operation;
    if (!event.details.empty())
    {
        line += " :: " + event.details;
    }
    return line;
}

static std::vector<std::string> formatKernelLogs(const BufferSink &sink)
{
    const auto events = sink.snapshot();
    std::vector<std::string> lines;
    lines.reserve(events.size());
    for (const auto &event : events)
    {
        lines.push_back(formatTraceEventLine(event));
    }

    return lines;
}

static std::string renderKernelTraceDump(const BufferSink &sink)
{
    const auto events = sink.snapshot();

    std::string dump;
    dump.reserve(events.size() * 64 + 128);
    dump += "\n=== Kernel Trace Dump ===\n";
    for (const auto &event : events)
    {
        dump += formatTraceEventLine(event);
        dump += '\n';
    }
    dump += "=== End Kernel Trace Dump (";
    dump += std::to_string(events.size());
    dump += " entries) ===\n";
    return dump;
}

static DemoKernelBuild buildDemoKernel()
{
    auto traceSink = std::make_shared<BufferSink>();

    auto clock = std::make_unique<SimulationClock>();
    auto tracerSink = std::make_unique<SharedBufferTraceSink>(traceSink);
    auto tracer = std::make_unique<Tracer>(std::move(tracerSink), *clock);
    auto memory = std::make_unique<PhysicalMemory>(64);
    auto replacement = std::make_unique<FifoReplacement>();
    auto mmu = std::make_unique<Mmu>(*memory, std::move(replacement), *tracer);
    auto virtualMem = std::make_unique<VirtualMemory>(*mmu, 1024);
    auto cpu = std::make_unique<Cpu>(*memory);
    auto engine = std::make_unique<InterpreterEngine>(*cpu, *memory);
    auto policy = std::make_unique<RoundRobinPolicy>(4);
    auto scheduler = std::make_unique<Scheduler>(std::move(policy), *tracer);
    auto syscallTable = std::make_unique<SyscallTable>();
    auto dispatcher = std::make_unique<Dispatcher>(*scheduler, *engine, *virtualMem, *clock, *tracer, *syscallTable);
    auto fs = std::make_unique<SimpleFS>(128);
    auto deviceManager = std::make_unique<DeviceManager>();
    auto ioManager = std::make_unique<IoManager>(*fs, *deviceManager);
    auto ipc = std::make_unique<IpcManager>();

    auto kernelResult = KernelBuilder{}
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
                            .withDefaultTickBudget(1)
                            .build();

    return DemoKernelBuild{std::move(kernelResult), std::move(traceSink)};
}

static void spawnDemoProcesses(IKernel &kernel)
{
    struct Demo
    {
        const char *name;
        PriorityLevel pri;
        std::int32_t nice;
        std::vector<Block> code;
    };

    const std::vector<Demo> demos = {
        {"calc-1-plus-1", PriorityLevel::Realtime, 0, makeProgramAddOnePlusOne()},
        {"counter", PriorityLevel::High, 0, makeProgramCounterLoop()},
        {"worker-1", PriorityLevel::Normal, 0, makeProgramCpuHeavy()},
        {"worker-2", PriorityLevel::Normal, 5, makeProgramLongNop(24)},
        {"background", PriorityLevel::Low, 10, makeProgramLongNop(48)},
        {"idle-task", PriorityLevel::Idle, 19, makeProgramLongNop(96)},
    };

    for (const auto &d : demos)
    {
        ProcessConfig cfg;
        cfg.name = d.name;
        cfg.priority = Priority{d.pri, d.pri, d.nice};
        cfg.code = d.code;
        (void)kernel.createProcess(cfg);
    }
}

int main()
{
    // Build kernel
    auto build = buildDemoKernel();
    if (build.kernelResult.isError())
    {
        std::cerr << "Failed to build kernel\n";
        return 1;
    }

    auto kernel = std::move(build.kernelResult).value();

    // Spawn demo processes
    spawnDemoProcesses(*kernel);

    // Wire up TUI stack
    KernelDiagnostics diagnostics(*kernel);
    KernelReadModel readModel(diagnostics);

    TuiController controller(readModel, [&kernel](std::size_t step) { return kernel->runForTicks(step); }, 512);

    // Launch interactive app
    FtxuiApp app(
        controller,
        FtxuiAppConfig{
            .defaultIntervalMs = 300,
            .defaultStep = 1,
            .frameIntervalMs = 33,
            .minIntervalMs = 50,
            .maxIntervalMs = 2000,
            .logProvider = [traceSink = build.traceSink] { return formatKernelLogs(*traceSink); },
        }
    );

    app.run();

    std::cout << renderKernelTraceDump(*build.traceSink);

    return 0;
}
