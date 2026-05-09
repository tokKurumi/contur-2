/// @file test_native_kernel_flow.cpp
/// @brief Integration test: full Kernel pipeline driving a real Win32 child process.
///
/// Wires `KernelBuilder` with `NativeEngine` instead of `InterpreterEngine`,
/// admits a process whose `nativePath` points at a system binary, and drives
/// the kernel ticks until the process exits naturally. This is the headline
/// "kernel runs a real x86 program" demonstration for Phase 15.

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "contur/core/clock.h"
#include "contur/core/error.h"

#include "contur/cpu/cpu.h"
#include "contur/dispatch/dispatcher.h"
#include "contur/execution/native_engine.h"
#include "contur/fs/simple_fs.h"
#include "contur/ipc/ipc_manager.h"
#include "contur/kernel/i_kernel.h"
#include "contur/kernel/kernel_builder.h"
#include "contur/memory/fifo_replacement.h"
#include "contur/memory/mmu.h"
#include "contur/memory/physical_memory.h"
#include "contur/memory/virtual_memory.h"
#include "contur/scheduling/round_robin_policy.h"
#include "contur/scheduling/scheduler.h"
#include "contur/syscall/syscall_table.h"
#include "contur/tracing/buffer_sink.h"
#include "contur/tracing/null_tracer.h"
#include "contur/tracing/tracer.h"

using namespace contur;

#if defined(_WIN32)

namespace {

    constexpr const char *kHostnameExe = "C:\\Windows\\System32\\hostname.exe";
    constexpr const char *kWhereExe = "C:\\Windows\\System32\\where.exe";

    /// @brief Composition root for an integration test kernel running NativeEngine.
    struct NativeKernelHarness
    {
        std::unique_ptr<IKernel> kernel;
        NativeEngine *engineRaw = nullptr; // non-owning view; kernel owns the engine
    };

    NativeKernelHarness buildNativeKernel(std::size_t tickBudget = 4)
    {
        auto clock = std::make_unique<SimulationClock>();
        auto tracer = std::make_unique<NullTracer>(*clock);
        auto memory = std::make_unique<PhysicalMemory>(64);
        auto mmu = std::make_unique<Mmu>(*memory, std::make_unique<FifoReplacement>(), *tracer);
        auto virtualMem = std::make_unique<VirtualMemory>(*mmu, MAX_PROCESSES);
        auto cpu = std::make_unique<Cpu>(*memory);

        auto engine = std::make_unique<NativeEngine>(*tracer, /*tickQuantumMs=*/2);
        NativeEngine *engineRaw = engine.get();

        auto policy = std::make_unique<RoundRobinPolicy>(tickBudget);
        auto scheduler = std::make_unique<Scheduler>(std::move(policy), *tracer);
        auto dispatcher = std::make_unique<Dispatcher>(*scheduler, *engine, *virtualMem, *clock, *tracer);

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
                                .withFileSystem(std::make_unique<SimpleFS>())
                                .withIpcManager(std::make_unique<IpcManager>())
                                .withSyscallTable(std::make_unique<SyscallTable>())
                                .withDefaultTickBudget(tickBudget)
                                .build();

        if (kernelResult.isError())
        {
            return NativeKernelHarness{};
        }
        return NativeKernelHarness{std::move(kernelResult).value(), engineRaw};
    }

    /// @brief Drives the kernel until all processes have exited or @p maxTicks reached.
    void drainUntilEmpty(IKernel &kernel, std::size_t maxTicks = 1000)
    {
        for (std::size_t i = 0; i < maxTicks; ++i)
        {
            if (kernel.processCount() == 0)
            {
                return;
            }
            auto r = kernel.tick(1);
            if (r.isError() && r.errorCode() == ErrorCode::NotFound)
            {
                return;
            }
        }
    }

} // namespace

// Basic native lifecycle: kernel drives a real exe to completion
TEST(NativeKernelFlowIntegrationTest, KernelRunsHostnameToCompletion)
{
    auto harness = buildNativeKernel();
    ASSERT_NE(harness.kernel, nullptr);
    ASSERT_NE(harness.engineRaw, nullptr);

    ProcessConfig cfg;
    cfg.name = "hostname";
    cfg.nativePath = kHostnameExe;
    auto created = harness.kernel->createProcess(cfg);
    ASSERT_TRUE(created.isOk()) << "createProcess failed: " << static_cast<int>(created.errorCode());
    const ProcessId pid = created.value();

    EXPECT_EQ(harness.kernel->processCount(), 1u);

    drainUntilEmpty(*harness.kernel);

    EXPECT_EQ(harness.kernel->processCount(), 0u) << "process did not exit within budget";
    // Stdout was captured by the engine before the kernel reaped the process.
    EXPECT_FALSE(harness.engineRaw->capturedStdout(pid).empty())
        << "expected hostname.exe to print at least its hostname";
}

TEST(NativeKernelFlowIntegrationTest, KernelHandlesMultipleNativeProcesses)
{
    auto harness = buildNativeKernel();
    ASSERT_NE(harness.kernel, nullptr);

    for (int i = 0; i < 3; ++i)
    {
        ProcessConfig cfg;
        cfg.name = "hostname-" + std::to_string(i);
        cfg.nativePath = kHostnameExe;
        ASSERT_TRUE(harness.kernel->createProcess(cfg).isOk());
    }

    EXPECT_EQ(harness.kernel->processCount(), 3u);
    drainUntilEmpty(*harness.kernel);
    EXPECT_EQ(harness.kernel->processCount(), 0u);
}

TEST(NativeKernelFlowIntegrationTest, NonZeroExitFromWhereIsObservable)
{
    auto harness = buildNativeKernel();
    ASSERT_NE(harness.kernel, nullptr);

    ProcessConfig cfg;
    cfg.name = "where-noargs";
    cfg.nativePath = kWhereExe;
    auto created = harness.kernel->createProcess(cfg);
    ASSERT_TRUE(created.isOk());

    drainUntilEmpty(*harness.kernel);
    EXPECT_EQ(harness.kernel->processCount(), 0u);
}

TEST(NativeKernelFlowIntegrationTest, MissingNativePathTerminatesQuickly)
{
    auto harness = buildNativeKernel();
    ASSERT_NE(harness.kernel, nullptr);

    ProcessConfig cfg;
    cfg.name = "ghost";
    cfg.nativePath = "C:\\nonexistent-binary-for-test.exe";
    auto created = harness.kernel->createProcess(cfg);
    ASSERT_TRUE(created.isOk()) << "createProcess should succeed; engine surfaces the error at dispatch";

    drainUntilEmpty(*harness.kernel, /*maxTicks=*/64);
    EXPECT_EQ(harness.kernel->processCount(), 0u) << "kernel should have reaped the process after spawn failure";
}

// Tracer integration — verify NativeEngine emits the lifecycle trail
TEST(NativeKernelFlowIntegrationTest, TraceSinkRecordsNativeLifecycle)
{
    auto sink = std::make_shared<BufferSink>();
    struct ForwardingSink final : public ITraceSink
    {
        std::shared_ptr<BufferSink> target;
        explicit ForwardingSink(std::shared_ptr<BufferSink> t)
            : target(std::move(t))
        {}
        void write(const TraceEvent &e) override
        {
            target->write(e);
        }
    };

    auto clock = std::make_unique<SimulationClock>();
    auto tracer = std::make_unique<Tracer>(std::make_unique<ForwardingSink>(sink), *clock);
    auto memory = std::make_unique<PhysicalMemory>(64);
    auto mmu = std::make_unique<Mmu>(*memory, std::make_unique<FifoReplacement>(), *tracer);
    auto virtualMem = std::make_unique<VirtualMemory>(*mmu, MAX_PROCESSES);
    auto cpu = std::make_unique<Cpu>(*memory);
    auto engine = std::make_unique<NativeEngine>(*tracer, /*tickQuantumMs=*/2);
    auto scheduler = std::make_unique<Scheduler>(std::make_unique<RoundRobinPolicy>(4), *tracer);
    auto dispatcher = std::make_unique<Dispatcher>(*scheduler, *engine, *virtualMem, *clock, *tracer);

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
                            .withFileSystem(std::make_unique<SimpleFS>())
                            .withIpcManager(std::make_unique<IpcManager>())
                            .withSyscallTable(std::make_unique<SyscallTable>())
                            .withDefaultTickBudget(4)
                            .build();
    ASSERT_TRUE(kernelResult.isOk());
    auto kernel = std::move(kernelResult).value();

    ProcessConfig cfg;
    cfg.name = "traced-hostname";
    cfg.nativePath = kHostnameExe;
    ASSERT_TRUE(kernel->createProcess(cfg).isOk());
    drainUntilEmpty(*kernel);
    EXPECT_EQ(kernel->processCount(), 0u);

    auto events = sink->snapshot();
    bool sawSpawn = false;
    bool sawExit = false;
    for (const auto &ev : events)
    {
        if (ev.subsystem == "NativeEngine" && ev.operation == "spawn.ok")
        {
            sawSpawn = true;
        }
        if (ev.subsystem == "NativeEngine" && (ev.operation == "exit" || ev.operation == "exit.report"))
        {
            sawExit = true;
        }
    }
    EXPECT_TRUE(sawSpawn);
    EXPECT_TRUE(sawExit);
}

#elif defined(__unix__) || defined(__APPLE__)

namespace {

    constexpr const char *kHostnameBin = "/bin/hostname";
    constexpr const char *kTrueBin = "/bin/true";
    constexpr const char *kFalseBin = "/bin/false";

    /// @brief Composition root for an integration test kernel running NativeEngine.
    struct NativeKernelHarness
    {
        std::unique_ptr<IKernel> kernel;
        NativeEngine *engineRaw = nullptr; // non-owning view; kernel owns the engine
    };

    NativeKernelHarness buildNativeKernel(std::size_t tickBudget = 4)
    {
        auto clock = std::make_unique<SimulationClock>();
        auto tracer = std::make_unique<NullTracer>(*clock);
        auto memory = std::make_unique<PhysicalMemory>(64);
        auto mmu = std::make_unique<Mmu>(*memory, std::make_unique<FifoReplacement>(), *tracer);
        auto virtualMem = std::make_unique<VirtualMemory>(*mmu, MAX_PROCESSES);
        auto cpu = std::make_unique<Cpu>(*memory);

        auto engine = std::make_unique<NativeEngine>(*tracer, /*tickQuantumMs=*/2);
        NativeEngine *engineRaw = engine.get();

        auto policy = std::make_unique<RoundRobinPolicy>(tickBudget);
        auto scheduler = std::make_unique<Scheduler>(std::move(policy), *tracer);
        auto dispatcher = std::make_unique<Dispatcher>(*scheduler, *engine, *virtualMem, *clock, *tracer);

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
                                .withFileSystem(std::make_unique<SimpleFS>())
                                .withIpcManager(std::make_unique<IpcManager>())
                                .withSyscallTable(std::make_unique<SyscallTable>())
                                .withDefaultTickBudget(tickBudget)
                                .build();
        if (kernelResult.isError())
        {
            return NativeKernelHarness{};
        }
        return NativeKernelHarness{std::move(kernelResult).value(), engineRaw};
    }

    void drainUntilEmpty(IKernel &kernel, std::size_t maxTicks = 1000)
    {
        for (std::size_t i = 0; i < maxTicks; ++i)
        {
            if (kernel.processCount() == 0)
            {
                return;
            }
            auto r = kernel.tick(1);
            if (r.isError() && r.errorCode() == ErrorCode::NotFound)
            {
                return;
            }
        }
    }

} // namespace

TEST(NativeKernelFlowIntegrationTest, KernelRunsHostnameToCompletion)
{
    auto harness = buildNativeKernel();
    ASSERT_NE(harness.kernel, nullptr);

    ProcessConfig cfg;
    cfg.name = "hostname";
    cfg.nativePath = kHostnameBin;
    auto created = harness.kernel->createProcess(cfg);
    ASSERT_TRUE(created.isOk()) << "createProcess failed: " << static_cast<int>(created.errorCode());
    const ProcessId pid = created.value();

    EXPECT_EQ(harness.kernel->processCount(), 1u);
    drainUntilEmpty(*harness.kernel);
    EXPECT_EQ(harness.kernel->processCount(), 0u);
    EXPECT_FALSE(harness.engineRaw->capturedStdout(pid).empty());
}

TEST(NativeKernelFlowIntegrationTest, KernelHandlesMultipleNativeProcesses)
{
    auto harness = buildNativeKernel();
    ASSERT_NE(harness.kernel, nullptr);

    for (int i = 0; i < 3; ++i)
    {
        ProcessConfig cfg;
        cfg.name = "true-" + std::to_string(i);
        cfg.nativePath = kTrueBin;
        ASSERT_TRUE(harness.kernel->createProcess(cfg).isOk());
    }

    EXPECT_EQ(harness.kernel->processCount(), 3u);
    drainUntilEmpty(*harness.kernel);
    EXPECT_EQ(harness.kernel->processCount(), 0u);
}

TEST(NativeKernelFlowIntegrationTest, NonZeroExitFromFalseIsObservable)
{
    auto harness = buildNativeKernel();
    ASSERT_NE(harness.kernel, nullptr);

    ProcessConfig cfg;
    cfg.name = "false";
    cfg.nativePath = kFalseBin;
    ASSERT_TRUE(harness.kernel->createProcess(cfg).isOk());

    drainUntilEmpty(*harness.kernel);
    EXPECT_EQ(harness.kernel->processCount(), 0u);
}

TEST(NativeKernelFlowIntegrationTest, MissingNativePathTerminatesQuickly)
{
    auto harness = buildNativeKernel();
    ASSERT_NE(harness.kernel, nullptr);

    ProcessConfig cfg;
    cfg.name = "ghost";
    cfg.nativePath = "/this/path/should/not/exist/contur-test-binary";
    ASSERT_TRUE(harness.kernel->createProcess(cfg).isOk());

    drainUntilEmpty(*harness.kernel, /*maxTicks=*/64);
    EXPECT_EQ(harness.kernel->processCount(), 0u);
}

TEST(NativeKernelFlowIntegrationTest, TraceSinkRecordsNativeLifecycle)
{
    auto sink = std::make_shared<BufferSink>();
    struct ForwardingSink final : public ITraceSink
    {
        std::shared_ptr<BufferSink> target;
        explicit ForwardingSink(std::shared_ptr<BufferSink> t)
            : target(std::move(t))
        {}
        void write(const TraceEvent &e) override
        {
            target->write(e);
        }
    };

    auto clock = std::make_unique<SimulationClock>();
    auto tracer = std::make_unique<Tracer>(std::make_unique<ForwardingSink>(sink), *clock);
    auto memory = std::make_unique<PhysicalMemory>(64);
    auto mmu = std::make_unique<Mmu>(*memory, std::make_unique<FifoReplacement>(), *tracer);
    auto virtualMem = std::make_unique<VirtualMemory>(*mmu, MAX_PROCESSES);
    auto cpu = std::make_unique<Cpu>(*memory);
    auto engine = std::make_unique<NativeEngine>(*tracer, /*tickQuantumMs=*/2);
    auto scheduler = std::make_unique<Scheduler>(std::make_unique<RoundRobinPolicy>(4), *tracer);
    auto dispatcher = std::make_unique<Dispatcher>(*scheduler, *engine, *virtualMem, *clock, *tracer);

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
                            .withFileSystem(std::make_unique<SimpleFS>())
                            .withIpcManager(std::make_unique<IpcManager>())
                            .withSyscallTable(std::make_unique<SyscallTable>())
                            .withDefaultTickBudget(4)
                            .build();
    ASSERT_TRUE(kernelResult.isOk());
    auto kernel = std::move(kernelResult).value();

    ProcessConfig cfg;
    cfg.name = "traced-true";
    cfg.nativePath = kTrueBin;
    ASSERT_TRUE(kernel->createProcess(cfg).isOk());
    drainUntilEmpty(*kernel);
    EXPECT_EQ(kernel->processCount(), 0u);

    auto events = sink->snapshot();
    bool sawSpawn = false;
    bool sawExit = false;
    for (const auto &ev : events)
    {
        if (ev.subsystem == "NativeEngine" && ev.operation == "spawn.ok")
        {
            sawSpawn = true;
        }
        if (ev.subsystem == "NativeEngine" && (ev.operation == "exit" || ev.operation == "exit.report"))
        {
            sawExit = true;
        }
    }
    EXPECT_TRUE(sawSpawn);
    EXPECT_TRUE(sawExit);
}

// C-Lang Hello World — the headline demo for the POSIX backend
//
// This test compiles a real C source file (`tests/fixtures/native_hello.c`)
// at CMake configure time using the host's C compiler, then drives it through
// the kernel pipeline via NativeEngine. End to end: a real ELF binary on disk
// is admitted as a Contur process, scheduled, run, and reaped. Stdout is
// captured and compared against the expected output.

#include "native_test_paths.h"

TEST(NativeKernelFlowIntegrationTest, KernelRunsCompiledClangHelloWorld)
{
    const std::string helloPath = contur::test_fixtures::kNativeHelloBinary;
    if (helloPath.empty())
    {
        GTEST_SKIP() << "No host C compiler was found at configure time; native_hello fixture not built.";
    }

    auto harness = buildNativeKernel();
    ASSERT_NE(harness.kernel, nullptr);

    ProcessConfig cfg;
    cfg.name = "hello-c";
    cfg.nativePath = helloPath;
    auto created = harness.kernel->createProcess(cfg);
    ASSERT_TRUE(created.isOk());
    const ProcessId pid = created.value();

    drainUntilEmpty(*harness.kernel);
    EXPECT_EQ(harness.kernel->processCount(), 0u);

    const std::string out = harness.engineRaw->capturedStdout(pid);
    EXPECT_NE(out.find("hello, contur"), std::string::npos)
        << "expected 'hello, contur' in captured stdout, got: " << out;
}

#endif // _WIN32 / __unix__
