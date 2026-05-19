/// @file main.cpp
/// @brief LAN File Relay Demo — two processes exchange a value through LAN files.
///
/// P2 reads F2 over a simulated LAN file, computes an arithmetic expression
/// (16/2*3 + 24 + 13 + F2_value) and stores the result in shared physical memory.
/// P1 polls shared memory until P2 sets a ready flag, then writes the result to F1.
///
/// Interaction: the user picks the target file for P1 and the initial value of F2.
/// The simulation runs in an FTXUI interactive TUI with autoplay / single-tick modes.

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "contur/core/clock.h"
#include "contur/core/error.h"

#include "contur/arch/block.h"
#include "contur/arch/interrupt.h"
#include "contur/arch/program_builder.h"
#include "contur/arch/register_file.h"
#include "contur/cpu/cpu.h"
#include "contur/dispatch/dispatcher.h"
#include "contur/execution/interpreter_engine.h"
#include "contur/fs/fs_utils.h"
#include "contur/fs/i_filesystem.h"
#include "contur/fs/simple_fs.h"
#include "contur/io/device_manager.h"
#include "contur/io/i_io_manager.h"
#include "contur/io/io_manager.h"
#include "contur/io/io_types.h"
#include "contur/io/network_device.h"
#include "contur/ipc/ipc_manager.h"
#include "contur/kernel/i_kernel.h"
#include "contur/kernel/kernel_builder.h"
#include "contur/kernel/kernel_diagnostics.h"
#include "contur/memory/fifo_replacement.h"
#include "contur/memory/mmu.h"
#include "contur/memory/physical_memory.h"
#include "contur/memory/virtual_memory.h"
#include "contur/process/priority.h"
#include "contur/scheduling/round_robin_policy.h"
#include "contur/scheduling/scheduler.h"
#include "contur/syscall/program_syscalls.h"
#include "contur/syscall/syscall_conventions.h"
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

    constexpr MemoryAddress kSharedValueAddr = 220;
    constexpr MemoryAddress kSharedFlagAddr = 221;
    constexpr RegisterValue kFlagReady = 1;

    constexpr std::uint8_t kRegFd = static_cast<std::uint8_t>(Register::R6);
    constexpr std::uint8_t kRegPayload = static_cast<std::uint8_t>(Register::R5);
    constexpr std::uint8_t kRegScratch = static_cast<std::uint8_t>(Register::R4);
    constexpr std::uint8_t kRegFlag = static_cast<std::uint8_t>(Register::R3);

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

    std::string formatTraceEventLine(const TraceEvent &event)
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

    std::vector<std::string> formatKernelLogs(const BufferSink &sink)
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

    struct DemoKernelBuild
    {
        std::unique_ptr<IKernel> kernel;
        std::shared_ptr<BufferSink> traceSink;
        IFileSystem *fileSystem = nullptr;
        IIoManager *ioManager = nullptr;
    };

    DemoKernelBuild buildDemoKernel(RegisterValue f2Value, const std::string &f1Path, const std::string &f2Path)
    {
        auto traceSink = std::make_shared<BufferSink>();

        auto clock = std::make_unique<SimulationClock>();
        auto tracerSink = std::make_unique<SharedBufferTraceSink>(traceSink);
        auto tracer = std::make_unique<Tracer>(std::move(tracerSink), *clock);
        auto memory = std::make_unique<PhysicalMemory>(512);
        auto replacement = std::make_unique<FifoReplacement>();
        auto mmu = std::make_unique<Mmu>(*memory, std::move(replacement), *tracer);
        auto virtualMemory = std::make_unique<VirtualMemory>(*mmu, 1024);
        auto cpu = std::make_unique<Cpu>(*memory);
        auto engine = std::make_unique<InterpreterEngine>(*cpu, *memory);
        auto policy = std::make_unique<RoundRobinPolicy>(2);
        auto scheduler = std::make_unique<Scheduler>(std::move(policy), *tracer);
        auto syscallTable = std::make_unique<SyscallTable>();
        auto dispatcher =
            std::make_unique<Dispatcher>(*scheduler, *engine, *virtualMemory, *clock, *tracer, *syscallTable);
        auto fileSystem = std::make_unique<SimpleFS>(256);
        auto deviceManager = std::make_unique<DeviceManager>();
        auto ioManager = std::make_unique<IoManager>(*fileSystem, *deviceManager);
        auto ipc = std::make_unique<IpcManager>();

        (void)deviceManager->registerDevice(std::make_unique<NetworkDevice>(128));
        (void)fileSystem->mkdir("/lan");
        (void)writeFileValue(*fileSystem, f1Path, 0);
        (void)writeFileValue(*fileSystem, f2Path, f2Value);
        (void)ioManager->registerFile(1, f1Path, IoResourceKind::LanFile);
        (void)ioManager->registerFile(2, f2Path, IoResourceKind::LanFile);

        IFileSystem *rawFs = fileSystem.get();
        IIoManager *rawIo = ioManager.get();

        auto kernelResult = KernelBuilder{}
                                .withClock(std::move(clock))
                                .withMemory(std::move(memory))
                                .withMmu(std::move(mmu))
                                .withVirtualMemory(std::move(virtualMemory))
                                .withCpu(std::move(cpu))
                                .withExecutionEngine(std::move(engine))
                                .withScheduler(std::move(scheduler))
                                .withDispatcher(std::move(dispatcher))
                                .withTracer(std::move(tracer))
                                .withFileSystem(std::move(fileSystem))
                                .withDeviceManager(std::move(deviceManager))
                                .withIoManager(std::move(ioManager))
                                .withIpcManager(std::move(ipc))
                                .withSyscallTable(std::move(syscallTable))
                                .withDefaultTickBudget(1)
                                .build();

        if (kernelResult.isError())
        {
            return DemoKernelBuild{nullptr, std::move(traceSink), nullptr, nullptr};
        }

        return DemoKernelBuild{
            std::move(kernelResult).value(),
            std::move(traceSink),
            rawFs,
            rawIo,
        };
    }

    std::vector<std::string> makeStatusLines(IFileSystem &fs, const std::string &f1Path, const std::string &f2Path)
    {
        auto f1 = readFileValue(fs, f1Path);
        auto f2 = readFileValue(fs, f2Path);

        auto f1Text = f1.isOk() ? std::to_string(f1.value()) : std::string(errorCodeToString(f1.errorCode()));
        auto f2Text = f2.isOk() ? std::to_string(f2.value()) : std::string(errorCodeToString(f2.errorCode()));

        return {"LAN F1: " + f1Text, "LAN F2: " + f2Text};
    }

    // P2: read F2 → compute → write result to shared memory
    //
    // Formula: 16 / 2 * 3 + 24 + 13 + F2_value
    // Result and ready-flag land at kSharedValueAddr / kSharedFlagAddr.
    std::vector<Block> makeP2Program(RegisterValue f2ResourceId)
    {
        std::vector<Block> code;

        emitOpen(code, f2ResourceId, IoResourceKind::LanFile, OpenMode::Read);
        code.push_back(movReg(kRegFd, SYSCALL_ID_REGISTER));

        emitRead(code, kRegFd);
        code.push_back(movReg(kRegPayload, SYSCALL_ID_REGISTER));

        // 16 / 2 * 3 + 24 + 13 + payload
        code.push_back(movImm(kRegScratch, 16));
        code.push_back(divImm(kRegScratch, 2));
        code.push_back(mulImm(kRegScratch, 3));
        code.push_back(addImm(kRegScratch, 24));
        code.push_back(addImm(kRegScratch, 13));
        code.push_back(addReg(kRegScratch, kRegPayload));

        code.push_back(writeMem(kRegScratch, kSharedValueAddr));
        code.push_back(movImm(kRegScratch, kFlagReady));
        code.push_back(writeMem(kRegScratch, kSharedFlagAddr));

        emitClose(code, kRegFd);
        code.push_back(interrupt(Interrupt::Exit));

        return code;
    }

    // P1: poll shared memory → write result to F1
    //
    // Spins until P2 sets the ready flag at kSharedFlagAddr, then writes
    // kSharedValueAddr to the target LAN file.
    std::vector<Block> makeP1Program(RegisterValue f1ResourceId)
    {
        std::vector<Block> code;

        constexpr RegisterValue loopStart = 0;

        // Polling loop: re-reads flag each iteration.
        code.push_back(readMem(kRegFlag, kSharedFlagAddr));
        code.push_back(compareImm(kRegFlag, kFlagReady));
        const std::size_t jumpIndex = code.size();
        code.push_back(jumpNotEqual(loopStart));

        emitOpen(code, f1ResourceId, IoResourceKind::LanFile, OpenMode::Create | OpenMode::Write | OpenMode::Truncate);
        code.push_back(movReg(kRegFd, SYSCALL_ID_REGISTER));

        code.push_back(readMem(kRegPayload, kSharedValueAddr));

        emitWrite(code, kRegFd, kRegPayload);
        emitClose(code, kRegFd);
        code.push_back(interrupt(Interrupt::Exit));

        code[jumpIndex].operand = loopStart;
        return code;
    }

} // namespace

int main()
{
    const std::string f1Path = "/lan/f1.txt";
    const std::string f2Path = "/lan/f2.txt";

    std::cout << "LAN File Relay Demo\n";
    std::cout << "Choose target file for P1 write (F1/F2). Default F1: ";
    std::string targetInput;
    std::getline(std::cin, targetInput);

    RegisterValue targetFileId = 1;
    if (targetInput == "F2" || targetInput == "f2")
    {
        targetFileId = 2;
    }

    std::cout << "Initial value for F2 (integer). Default 7: ";
    std::string f2ValueInput;
    std::getline(std::cin, f2ValueInput);
    RegisterValue f2Value = 7;
    if (!f2ValueInput.empty())
    {
        auto parsed = parseRegisterValue(f2ValueInput);
        if (parsed.isOk())
        {
            f2Value = parsed.value();
        }
    }

    auto build = buildDemoKernel(f2Value, f1Path, f2Path);
    if (!build.kernel)
    {
        std::cerr << "Failed to build kernel\n";
        return 1;
    }

    auto &kernel = build.kernel;

    ProcessConfig p2Config;
    p2Config.name = "P2-reader";
    p2Config.priority = Priority{PriorityLevel::High, PriorityLevel::High, 0};
    p2Config.code = makeP2Program(2);

    ProcessConfig p1Config;
    p1Config.name = "P1-writer";
    p1Config.priority = Priority{PriorityLevel::Normal, PriorityLevel::Normal, 0};
    p1Config.code = makeP1Program(targetFileId);

    if (kernel->createProcess(p2Config).isError() || kernel->createProcess(p1Config).isError())
    {
        std::cerr << "Failed to create demo processes\n";
        return 1;
    }

    KernelDiagnostics diagnostics(*kernel);
    KernelReadModel readModel(diagnostics);
    TuiController controller(readModel, [&kernel](std::size_t step) { return kernel->runForTicks(step); }, 512);

    FtxuiApp app(
        controller,
        FtxuiAppConfig{
            .defaultIntervalMs = 300,
            .defaultStep = 1,
            .frameIntervalMs = 33,
            .minIntervalMs = 50,
            .maxIntervalMs = 2000,
            .logProvider = [traceSink = build.traceSink, fs = build.fileSystem, f1Path, f2Path] {
                auto lines = formatKernelLogs(*traceSink);
                if (fs)
                {
                    auto status = makeStatusLines(*fs, f1Path, f2Path);
                    lines.insert(lines.end(), status.begin(), status.end());
                }
                return lines;
            },
        }
    );

    app.run();
    return 0;
}
