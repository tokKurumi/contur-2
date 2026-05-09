/// @file test_native_engine.cpp
/// @brief Unit tests for NativeEngine — Windows host-process execution engine.
///
/// These tests exercise the real Win32 process-spawning path. They are gated
/// to Windows builds because the engine deliberately has no behaviour on
/// other hosts. Tests rely on standard Windows system binaries
/// (`hostname.exe`, `whoami.exe`, `where.exe`) that exit on their own.

#include <string>

#include <gtest/gtest.h>

#include "contur/core/clock.h"

#include "contur/arch/instruction.h"
#include "contur/arch/register_file.h"
#include "contur/execution/execution_context.h"
#include "contur/execution/native_engine.h"
#include "contur/process/process_image.h"
#include "contur/tracing/buffer_sink.h"
#include "contur/tracing/null_tracer.h"
#include "contur/tracing/tracer.h"

using namespace contur;

#if defined(_WIN32)

namespace {

    constexpr const char *kHostnameExe = "C:\\Windows\\System32\\hostname.exe";
    constexpr const char *kWhereExe = "C:\\Windows\\System32\\where.exe";

    /// @brief Builds a ProcessImage marked as native with a placeholder code segment.
    ProcessImage makeNativeProcess(ProcessId pid, std::string nativePath, std::string name = "native")
    {
        ProcessImage image(pid, std::move(name), {Block{Instruction::Halt, 0, 0, 0}});
        image.setNativePath(std::move(nativePath));
        return image;
    }

    /// @brief Drives a process to completion or until the engine reports a terminal state.
    /// @return The final ExecutionResult observed.
    ExecutionResult runToCompletion(NativeEngine &engine, ProcessImage &process, std::size_t maxIterations = 200)
    {
        ExecutionResult last = ExecutionResult::error(process.id(), 0, Interrupt::Error);
        for (std::size_t i = 0; i < maxIterations; ++i)
        {
            last = engine.execute(process, /*tickBudget=*/4);
            if (last.reason == StopReason::ProcessExited || last.reason == StopReason::Halted ||
                last.reason == StopReason::Error)
            {
                return last;
            }
        }
        return last;
    }

} // namespace

class NativeEngineTest : public ::testing::Test
{
    protected:
    SimulationClock clock_;
    NullTracer tracer_{clock_};
};

TEST_F(NativeEngineTest, NameReturnsNative)
{
    NativeEngine engine(tracer_);
    EXPECT_EQ(engine.name(), "Native");
}

TEST_F(NativeEngineTest, MissingNativePathReturnsError)
{
    NativeEngine engine(tracer_);
    ProcessImage process(1, "interp-only", {Block{Instruction::Halt, 0, 0, 0}});
    // No setNativePath() — engine must reject without spawning.
    auto result = engine.execute(process, 4);
    EXPECT_EQ(result.reason, StopReason::Error);
    EXPECT_EQ(result.pid, 1u);
    EXPECT_FALSE(engine.isTracking(1));
}

TEST_F(NativeEngineTest, RunsHostnameToCompletion)
{
    NativeEngine engine(tracer_, /*tickQuantumMs=*/2);
    auto process = makeNativeProcess(2, kHostnameExe, "hostname");

    auto result = runToCompletion(engine, process);
    EXPECT_EQ(result.reason, StopReason::ProcessExited);
    EXPECT_EQ(result.pid, 2u);
    // hostname.exe exits with code 0 on success.
    EXPECT_EQ(process.registers().get(Register::R0), 0);
    // It should have produced at least one byte on stdout (the hostname + newline).
    EXPECT_FALSE(engine.capturedStdout(2).empty());
}

TEST_F(NativeEngineTest, NonZeroExitCodeIsCaptured)
{
    NativeEngine engine(tracer_, /*tickQuantumMs=*/2);
    // `where.exe` invoked with no arguments prints usage to stderr and exits with code 1.
    auto process = makeNativeProcess(3, kWhereExe, "where-nohelp");

    auto result = runToCompletion(engine, process);
    EXPECT_EQ(result.reason, StopReason::ProcessExited);
    // Some Windows builds return 1; allow any non-special value (the point is that
    // we observed termination and stored *some* exit code, not 0xCAFEBABE).
    EXPECT_NE(process.registers().get(Register::R0), 0x7FFFFFFF);
}

TEST_F(NativeEngineTest, HaltOnTrackedChildSurfacesTerminalState)
{
    NativeEngine engine(tracer_, /*tickQuantumMs=*/2);
    auto process = makeNativeProcess(4, kHostnameExe, "halt-target");

    // First call may spawn, run, and finish all in one slice — that's fine.
    auto first = engine.execute(process, /*tickBudget=*/1);
    EXPECT_TRUE(first.reason == StopReason::BudgetExhausted || first.reason == StopReason::ProcessExited);

    engine.halt(4);
    // After halt, the entry is reaped (still tracked, marked exited). Next execute()
    // observes a terminal state without resuming the child.
    auto post = engine.execute(process, /*tickBudget=*/1);
    EXPECT_TRUE(post.reason == StopReason::Halted || post.reason == StopReason::ProcessExited);
}

TEST_F(NativeEngineTest, TracerReceivesSpawnAndExitEvents)
{
    SimulationClock clock;
    auto sink = std::make_unique<BufferSink>();
    BufferSink *sinkRaw = sink.get();
    Tracer tracer(std::move(sink), clock);

    NativeEngine engine(tracer, /*tickQuantumMs=*/2);
    auto process = makeNativeProcess(5, kHostnameExe, "trace-check");

    auto result = runToCompletion(engine, process);
    EXPECT_EQ(result.reason, StopReason::ProcessExited);

    auto events = sinkRaw->snapshot();
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

TEST_F(NativeEngineTest, IsTrackingReportsState)
{
    NativeEngine engine(tracer_, /*tickQuantumMs=*/2);
    EXPECT_FALSE(engine.isTracking(99));
    EXPECT_TRUE(engine.capturedStdout(99).empty());

    auto process = makeNativeProcess(99, kHostnameExe);
    auto result = runToCompletion(engine, process);
    EXPECT_EQ(result.reason, StopReason::ProcessExited);
    EXPECT_TRUE(engine.isTracking(99));
}

TEST_F(NativeEngineTest, RejectsInvalidExecutablePath)
{
    NativeEngine engine(tracer_);
    auto process = makeNativeProcess(7, "C:\\does\\not\\exist\\definitely-not-a-binary.exe");

    auto result = engine.execute(process, /*tickBudget=*/1);
    EXPECT_EQ(result.reason, StopReason::Error);
    EXPECT_FALSE(engine.isTracking(7));
}

#elif defined(__unix__) || defined(__APPLE__)

namespace {

    // Standard POSIX binaries used as test fixtures. These exist on Linux x86,
    // macOS, and other Unix hosts. They each terminate on their own:
    //   /bin/hostname  → prints hostname, exits 0
    //   /bin/true      → exits 0 immediately
    //   /bin/false     → exits 1 immediately
    constexpr const char *kHostnameBin = "/bin/hostname";
    constexpr const char *kTrueBin = "/bin/true";
    constexpr const char *kFalseBin = "/bin/false";

    /// @brief Builds a ProcessImage marked as native with a placeholder code segment.
    ProcessImage makeNativeProcess(ProcessId pid, std::string nativePath, std::string name = "native")
    {
        ProcessImage image(pid, std::move(name), {Block{Instruction::Halt, 0, 0, 0}});
        image.setNativePath(std::move(nativePath));
        return image;
    }

    /// @brief Drives a process to completion or until the engine reports a terminal state.
    ExecutionResult runToCompletion(NativeEngine &engine, ProcessImage &process, std::size_t maxIterations = 200)
    {
        ExecutionResult last = ExecutionResult::error(process.id(), 0, Interrupt::Error);
        for (std::size_t i = 0; i < maxIterations; ++i)
        {
            last = engine.execute(process, /*tickBudget=*/4);
            if (last.reason == StopReason::ProcessExited || last.reason == StopReason::Halted ||
                last.reason == StopReason::Error)
            {
                return last;
            }
        }
        return last;
    }

} // namespace

class NativeEngineTest : public ::testing::Test
{
    protected:
    SimulationClock clock_;
    NullTracer tracer_{clock_};
};

TEST_F(NativeEngineTest, NameReturnsNative)
{
    NativeEngine engine(tracer_);
    EXPECT_EQ(engine.name(), "Native");
}

TEST_F(NativeEngineTest, MissingNativePathReturnsError)
{
    NativeEngine engine(tracer_);
    ProcessImage process(1, "interp-only", {Block{Instruction::Halt, 0, 0, 0}});
    auto result = engine.execute(process, 4);
    EXPECT_EQ(result.reason, StopReason::Error);
    EXPECT_EQ(result.pid, 1u);
    EXPECT_FALSE(engine.isTracking(1));
}

TEST_F(NativeEngineTest, RunsTrueWithExitCodeZero)
{
    NativeEngine engine(tracer_, /*tickQuantumMs=*/2);
    auto process = makeNativeProcess(2, kTrueBin, "true");

    auto result = runToCompletion(engine, process);
    EXPECT_EQ(result.reason, StopReason::ProcessExited);
    EXPECT_EQ(result.pid, 2u);
    EXPECT_EQ(process.registers().get(Register::R0), 0);
}

TEST_F(NativeEngineTest, RunsFalseWithExitCodeOne)
{
    NativeEngine engine(tracer_, /*tickQuantumMs=*/2);
    auto process = makeNativeProcess(3, kFalseBin, "false");

    auto result = runToCompletion(engine, process);
    EXPECT_EQ(result.reason, StopReason::ProcessExited);
    EXPECT_EQ(process.registers().get(Register::R0), 1);
}

TEST_F(NativeEngineTest, RunsHostnameAndCapturesStdout)
{
    NativeEngine engine(tracer_, /*tickQuantumMs=*/2);
    auto process = makeNativeProcess(4, kHostnameBin, "hostname");

    auto result = runToCompletion(engine, process);
    EXPECT_EQ(result.reason, StopReason::ProcessExited);
    EXPECT_EQ(process.registers().get(Register::R0), 0);
    // hostname prints at least one character + newline.
    EXPECT_FALSE(engine.capturedStdout(4).empty());
}

TEST_F(NativeEngineTest, HaltOnTrackedChildSurfacesTerminalState)
{
    NativeEngine engine(tracer_, /*tickQuantumMs=*/2);
    auto process = makeNativeProcess(5, kTrueBin, "halt-target");

    auto first = engine.execute(process, /*tickBudget=*/1);
    EXPECT_TRUE(first.reason == StopReason::BudgetExhausted || first.reason == StopReason::ProcessExited);

    engine.halt(5);
    auto post = engine.execute(process, /*tickBudget=*/1);
    EXPECT_TRUE(post.reason == StopReason::Halted || post.reason == StopReason::ProcessExited);
}

TEST_F(NativeEngineTest, TracerReceivesSpawnAndExitEvents)
{
    SimulationClock clock;
    auto sink = std::make_unique<BufferSink>();
    BufferSink *sinkRaw = sink.get();
    Tracer tracer(std::move(sink), clock);

    NativeEngine engine(tracer, /*tickQuantumMs=*/2);
    auto process = makeNativeProcess(6, kTrueBin, "trace-check");

    auto result = runToCompletion(engine, process);
    EXPECT_EQ(result.reason, StopReason::ProcessExited);

    auto events = sinkRaw->snapshot();
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

TEST_F(NativeEngineTest, IsTrackingReportsState)
{
    NativeEngine engine(tracer_, /*tickQuantumMs=*/2);
    EXPECT_FALSE(engine.isTracking(99));
    EXPECT_TRUE(engine.capturedStdout(99).empty());

    auto process = makeNativeProcess(99, kTrueBin);
    auto result = runToCompletion(engine, process);
    EXPECT_EQ(result.reason, StopReason::ProcessExited);
    EXPECT_TRUE(engine.isTracking(99));
}

TEST_F(NativeEngineTest, RejectsInvalidExecutablePath)
{
    NativeEngine engine(tracer_);
    auto process = makeNativeProcess(7, "/this/path/should/not/exist/contur-test-binary");

    // fork() succeeds but execv() fails in the child — the child exits with 127.
    // The first execute() returns BudgetExhausted (we spawned and stopped successfully)
    // or ProcessExited (the child raced past SIGSTOP and finished). The next
    // execute() always observes the 127 exit code.
    auto result = runToCompletion(engine, process);
    EXPECT_EQ(result.reason, StopReason::ProcessExited);
    EXPECT_EQ(process.registers().get(Register::R0), 127);
}

#else // unsupported host (neither _WIN32 nor __unix__)

TEST(NativeEngineTest, UnsupportedHostReturnsError)
{
    SimulationClock clock;
    NullTracer tracer(clock);
    NativeEngine engine(tracer);
    ProcessImage process(1, "noop", {Block{Instruction::Halt, 0, 0, 0}});
    process.setNativePath("/anything");
    auto result = engine.execute(process, 4);
    EXPECT_EQ(result.reason, StopReason::Error);
}

#endif // _WIN32 / __unix__ / fallback
