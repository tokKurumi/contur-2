/// @file test_interpreter_engine.cpp
/// @brief Unit tests for InterpreterEngine — bytecode interpreter execution engine.

#include <gtest/gtest.h>

#include "contur/arch/block.h"
#include "contur/arch/instruction.h"
#include "contur/arch/interrupt.h"
#include "contur/arch/register_file.h"
#include "contur/cpu/cpu.h"
#include "contur/execution/execution_context.h"
#include "contur/execution/interpreter_engine.h"
#include "contur/memory/physical_memory.h"
#include "contur/process/process_image.h"

using namespace contur;

/// @brief Test fixture for InterpreterEngine tests.
///
/// Sets up a 256-cell physical memory, a CPU, and an InterpreterEngine.
class InterpreterEngineTest : public ::testing::Test
{
    protected:
    PhysicalMemory memory{256};
    Cpu cpu{memory};
    InterpreterEngine engine{cpu, memory};

    /// @brief Helper: creates a ProcessImage with the given code, starting PC at 0.
    ProcessImage makeProcess(ProcessId id, const std::string &name, std::vector<Block> code)
    {
        return ProcessImage(id, name, std::move(code));
    }
};

TEST_F(InterpreterEngineTest, NameReturnsInterpreter)
{
    EXPECT_EQ(engine.name(), "Interpreter");
}

TEST_F(InterpreterEngineTest, SimpleMovAndExit)
{
    auto process = makeProcess(
        1,
        "test",
        {
            Block{Instruction::Mov, 1, 42},
            Block{Instruction::Halt, 0, 0},
        }
    );

    auto result = engine.execute(process, 10);

    EXPECT_EQ(result.reason, StopReason::ProcessExited);
    EXPECT_EQ(result.interrupt, Interrupt::Exit);
    EXPECT_EQ(result.ticksConsumed, 2);
    EXPECT_EQ(result.pid, 1);
    EXPECT_EQ(process.registers().get(Register::R1), 42);
}

TEST_F(InterpreterEngineTest, MovAddAndExit)
{
    auto process = makeProcess(
        1,
        "add_test",
        {
            Block{Instruction::Mov, 0, 10},   // R0 = 10
            Block{Instruction::Mov, 1, 20},   // R1 = 20
            Block{Instruction::Add, 0, 1, 1}, // R0 += R1 (register mode)
            Block{Instruction::Halt, 0, 0},
        }
    );

    auto result = engine.execute(process, 10);

    EXPECT_EQ(result.reason, StopReason::ProcessExited);
    EXPECT_EQ(result.ticksConsumed, 4);
    EXPECT_EQ(process.registers().get(Register::R0), 30);
}

TEST_F(InterpreterEngineTest, BudgetExhaustedReturnsPreemption)
{
    auto process = makeProcess(
        1,
        "loop",
        {
            Block{Instruction::Mov, 0, 1},
            Block{Instruction::Add, 0, 1},
            Block{Instruction::Add, 0, 1},
            Block{Instruction::Add, 0, 1},
            Block{Instruction::Halt, 0, 0},
        }
    );

    auto result = engine.execute(process, 3);

    EXPECT_EQ(result.reason, StopReason::BudgetExhausted);
    EXPECT_EQ(result.ticksConsumed, 3);
    EXPECT_EQ(result.pid, 1);
    // R0 = 1 + 1 + 1 = 3 (Mov set 1, two Adds)
    EXPECT_EQ(process.registers().get(Register::R0), 3);
}

TEST_F(InterpreterEngineTest, ResumeAfterBudgetExhaustion)
{
    auto process = makeProcess(
        1,
        "resume",
        {
            Block{Instruction::Mov, 0, 10}, // tick 0
            Block{Instruction::Add, 0, 5},  // tick 1
            Block{Instruction::Add, 0, 3},  // tick 2
            Block{Instruction::Halt, 0, 0}, // tick 3
        }
    );

    // First burst: 2 ticks
    auto result1 = engine.execute(process, 2);
    EXPECT_EQ(result1.reason, StopReason::BudgetExhausted);
    EXPECT_EQ(result1.ticksConsumed, 2);
    EXPECT_EQ(process.registers().get(Register::R0), 15); // 10 + 5
    EXPECT_EQ(process.registers().pc(), 2);

    // Second burst: resume from where we left off
    auto result2 = engine.execute(process, 10);
    EXPECT_EQ(result2.reason, StopReason::ProcessExited);
    EXPECT_EQ(result2.ticksConsumed, 2);
    EXPECT_EQ(process.registers().get(Register::R0), 18); // 15 + 3
}

TEST_F(InterpreterEngineTest, ZeroBudgetReturnsImmediately)
{
    auto process = makeProcess(
        1,
        "zero_budget",
        {
            Block{Instruction::Mov, 0, 42},
            Block{Instruction::Halt, 0, 0},
        }
    );

    auto result = engine.execute(process, 0);

    EXPECT_EQ(result.reason, StopReason::BudgetExhausted);
    EXPECT_EQ(result.ticksConsumed, 0);
    // Nothing should have been executed
    EXPECT_EQ(process.registers().get(Register::R0), 0);
}

TEST_F(InterpreterEngineTest, IntExitTerminatesProcess)
{
    auto process = makeProcess(
        1,
        "int_exit",
        {
            Block{Instruction::Mov, 0, 99},
            Block{Instruction::Interrupt, 0, static_cast<std::int32_t>(Interrupt::Exit)},
            Block{Instruction::Mov, 1, 42}, // Should not be reached
        }
    );

    auto result = engine.execute(process, 10);

    EXPECT_EQ(result.reason, StopReason::ProcessExited);
    EXPECT_EQ(result.ticksConsumed, 2);
    EXPECT_EQ(process.registers().get(Register::R0), 99);
    EXPECT_EQ(process.registers().get(Register::R1), 0); // Not reached
}

TEST_F(InterpreterEngineTest, DivisionByZeroReturnsError)
{
    auto process = makeProcess(
        1,
        "div_zero",
        {
            Block{Instruction::Mov, 0, 10},
            Block{Instruction::Mov, 1, 0},
            Block{Instruction::Div, 0, 1, 1}, // R0 / R1 (R1 = 0)
        }
    );

    auto result = engine.execute(process, 10);

    EXPECT_EQ(result.reason, StopReason::Error);
    EXPECT_EQ(result.interrupt, Interrupt::DivByZero);
    EXPECT_EQ(result.ticksConsumed, 3);
}

TEST_F(InterpreterEngineTest, SyscallInterruptReturnsInterrupted)
{
    auto process = makeProcess(
        1,
        "syscall",
        {
            Block{Instruction::Mov, 0, 1},
            Block{Instruction::Interrupt, 0, static_cast<std::int32_t>(Interrupt::SystemCall)},
            Block{Instruction::Halt, 0, 0},
        }
    );

    auto result = engine.execute(process, 10);

    EXPECT_EQ(result.reason, StopReason::Interrupted);
    EXPECT_EQ(result.interrupt, Interrupt::SystemCall);
    EXPECT_EQ(result.ticksConsumed, 2);
}

TEST_F(InterpreterEngineTest, DeviceIOInterruptReturnsInterrupted)
{
    auto process = makeProcess(
        1,
        "device_io",
        {
            Block{Instruction::Interrupt, 0, static_cast<std::int32_t>(Interrupt::DeviceIO)},
        }
    );

    auto result = engine.execute(process, 10);

    EXPECT_EQ(result.reason, StopReason::Interrupted);
    EXPECT_EQ(result.interrupt, Interrupt::DeviceIO);
    EXPECT_EQ(result.ticksConsumed, 1);
}

TEST_F(InterpreterEngineTest, NetworkIOInterruptReturnsInterrupted)
{
    auto process = makeProcess(
        1,
        "net_io",
        {
            Block{Instruction::Interrupt, 0, static_cast<std::int32_t>(Interrupt::NetworkIO)},
        }
    );

    auto result = engine.execute(process, 10);

    EXPECT_EQ(result.reason, StopReason::Interrupted);
    EXPECT_EQ(result.interrupt, Interrupt::NetworkIO);
    EXPECT_EQ(result.ticksConsumed, 1);
}

TEST_F(InterpreterEngineTest, PageFaultInterruptReturnsInterrupted)
{
    auto process = makeProcess(
        1,
        "page_fault",
        {
            Block{Instruction::Interrupt, 0, static_cast<std::int32_t>(Interrupt::PageFault)},
        }
    );

    auto result = engine.execute(process, 10);

    EXPECT_EQ(result.reason, StopReason::Interrupted);
    EXPECT_EQ(result.interrupt, Interrupt::PageFault);
    EXPECT_EQ(result.ticksConsumed, 1);
}

TEST_F(InterpreterEngineTest, TimerInterruptReturnsInterrupted)
{
    auto process = makeProcess(
        1,
        "timer",
        {
            Block{Instruction::Interrupt, 0, static_cast<std::int32_t>(Interrupt::Timer)},
        }
    );

    auto result = engine.execute(process, 10);

    EXPECT_EQ(result.reason, StopReason::Interrupted);
    EXPECT_EQ(result.interrupt, Interrupt::Timer);
    EXPECT_EQ(result.ticksConsumed, 1);
}

TEST_F(InterpreterEngineTest, HaltBeforeExecutionReturnsHalted)
{
    auto process = makeProcess(
        1,
        "halted",
        {
            Block{Instruction::Mov, 0, 42},
            Block{Instruction::Halt, 0, 0},
        }
    );

    engine.halt(1);
    auto result = engine.execute(process, 10);

    EXPECT_EQ(result.reason, StopReason::Halted);
    EXPECT_EQ(result.ticksConsumed, 0);
    EXPECT_EQ(result.pid, 1);
    // No instructions executed
    EXPECT_EQ(process.registers().get(Register::R0), 0);
}

TEST_F(InterpreterEngineTest, HaltOnlyAffectsTargetPid)
{
    auto process1 = makeProcess(
        1,
        "p1",
        {
            Block{Instruction::Mov, 0, 42},
            Block{Instruction::Halt, 0, 0},
        }
    );

    auto process2 = makeProcess(
        2,
        "p2",
        {
            Block{Instruction::Mov, 0, 99},
            Block{Instruction::Halt, 0, 0},
        }
    );

    engine.halt(2); // halt process 2

    // Process 1 should run normally
    auto result1 = engine.execute(process1, 10);
    EXPECT_EQ(result1.reason, StopReason::ProcessExited);
    EXPECT_EQ(process1.registers().get(Register::R0), 42);

    // Process 2 should be halted
    auto result2 = engine.execute(process2, 10);
    EXPECT_EQ(result2.reason, StopReason::Halted);
    EXPECT_EQ(process2.registers().get(Register::R0), 0);
}

TEST_F(InterpreterEngineTest, HaltIsClearedAfterUse)
{
    auto process = makeProcess(
        1,
        "once",
        {
            Block{Instruction::Mov, 0, 42},
            Block{Instruction::Halt, 0, 0},
        }
    );

    engine.halt(1);

    // First attempt: halted
    auto result1 = engine.execute(process, 10);
    EXPECT_EQ(result1.reason, StopReason::Halted);

    // Second attempt: should now run normally (halt cleared)
    auto result2 = engine.execute(process, 10);
    EXPECT_EQ(result2.reason, StopReason::ProcessExited);
}

TEST_F(InterpreterEngineTest, ArithmeticSequence)
{
    auto process = makeProcess(
        1,
        "arith",
        {
            Block{Instruction::Mov, 0, 100},  // R0 = 100
            Block{Instruction::Mov, 1, 50},   // R1 = 50
            Block{Instruction::Sub, 0, 1, 1}, // R0 -= R1 → 50
            Block{Instruction::Mul, 0, 3},    // R0 *= 3 → 150
            Block{Instruction::Div, 0, 2},    // R0 /= 2 → 75
            Block{Instruction::Halt, 0, 0},
        }
    );

    auto result = engine.execute(process, 20);

    EXPECT_EQ(result.reason, StopReason::ProcessExited);
    EXPECT_EQ(result.ticksConsumed, 6);
    EXPECT_EQ(process.registers().get(Register::R0), 75);
}

TEST_F(InterpreterEngineTest, NopOnlyProgramExhaustsBudget)
{
    auto process = makeProcess(
        1,
        "nops",
        {
            Block{Instruction::Nop, 0, 0},
            Block{Instruction::Nop, 0, 0},
            Block{Instruction::Nop, 0, 0},
            Block{Instruction::Nop, 0, 0},
            Block{Instruction::Nop, 0, 0},
        }
    );

    auto result = engine.execute(process, 3);

    EXPECT_EQ(result.reason, StopReason::BudgetExhausted);
    EXPECT_EQ(result.ticksConsumed, 3);
    EXPECT_EQ(process.registers().pc(), 3);
}

TEST_F(InterpreterEngineTest, SingleInstructionBudget)
{
    auto process = makeProcess(
        1,
        "single",
        {
            Block{Instruction::Mov, 0, 42},
            Block{Instruction::Mov, 1, 99},
            Block{Instruction::Halt, 0, 0},
        }
    );

    auto result = engine.execute(process, 1);

    EXPECT_EQ(result.reason, StopReason::BudgetExhausted);
    EXPECT_EQ(result.ticksConsumed, 1);
    EXPECT_EQ(process.registers().get(Register::R0), 42);
    EXPECT_EQ(process.registers().get(Register::R1), 0); // Not yet executed
}

TEST_F(InterpreterEngineTest, ExecutionResultFactoryBudgetExhausted)
{
    auto result = ExecutionResult::budgetExhausted(5, 10);
    EXPECT_EQ(result.reason, StopReason::BudgetExhausted);
    EXPECT_EQ(result.interrupt, Interrupt::Ok);
    EXPECT_EQ(result.ticksConsumed, 10);
    EXPECT_EQ(result.pid, 5);
}

TEST_F(InterpreterEngineTest, ExecutionResultFactoryExited)
{
    auto result = ExecutionResult::exited(3, 7);
    EXPECT_EQ(result.reason, StopReason::ProcessExited);
    EXPECT_EQ(result.interrupt, Interrupt::Exit);
    EXPECT_EQ(result.ticksConsumed, 7);
    EXPECT_EQ(result.pid, 3);
}

TEST_F(InterpreterEngineTest, ExecutionResultFactoryError)
{
    auto result = ExecutionResult::error(2, 5, Interrupt::DivByZero);
    EXPECT_EQ(result.reason, StopReason::Error);
    EXPECT_EQ(result.interrupt, Interrupt::DivByZero);
    EXPECT_EQ(result.ticksConsumed, 5);
    EXPECT_EQ(result.pid, 2);
}

TEST_F(InterpreterEngineTest, ExecutionResultFactoryInterrupted)
{
    auto result = ExecutionResult::interrupted(4, 3, Interrupt::SystemCall);
    EXPECT_EQ(result.reason, StopReason::Interrupted);
    EXPECT_EQ(result.interrupt, Interrupt::SystemCall);
    EXPECT_EQ(result.ticksConsumed, 3);
    EXPECT_EQ(result.pid, 4);
}

TEST_F(InterpreterEngineTest, ExecutionResultFactoryHalted)
{
    auto result = ExecutionResult::halted(6, 2);
    EXPECT_EQ(result.reason, StopReason::Halted);
    EXPECT_EQ(result.interrupt, Interrupt::Exit);
    EXPECT_EQ(result.ticksConsumed, 2);
    EXPECT_EQ(result.pid, 6);
}

TEST_F(InterpreterEngineTest, ProcessWithNonZeroStartPC)
{
    // Code has 4 instructions, but PC starts at 2 (skipping the first two).
    auto process = makeProcess(
        1,
        "offset",
        {
            Block{Instruction::Mov, 0, 99}, // index 0 — skipped
            Block{Instruction::Mov, 1, 88}, // index 1 — skipped
            Block{Instruction::Mov, 2, 42}, // index 2 — executed first
            Block{Instruction::Halt, 0, 0}, // index 3 — exit
        }
    );

    process.registers().setPc(2);

    auto result = engine.execute(process, 10);

    EXPECT_EQ(result.reason, StopReason::ProcessExited);
    EXPECT_EQ(result.ticksConsumed, 2);
    EXPECT_EQ(process.registers().get(Register::R0), 0);  // Not set (skipped)
    EXPECT_EQ(process.registers().get(Register::R1), 0);  // Not set (skipped)
    EXPECT_EQ(process.registers().get(Register::R2), 42); // Set by Mov at index 2
}

TEST_F(InterpreterEngineTest, MultipleProcessesSequentially)
{
    auto p1 = makeProcess(
        1,
        "p1",
        {
            Block{Instruction::Mov, 0, 10},
            Block{Instruction::Halt, 0, 0},
        }
    );

    auto p2 = makeProcess(
        2,
        "p2",
        {
            Block{Instruction::Mov, 0, 20},
            Block{Instruction::Halt, 0, 0},
        }
    );

    auto r1 = engine.execute(p1, 10);
    EXPECT_EQ(r1.reason, StopReason::ProcessExited);
    EXPECT_EQ(r1.pid, 1);
    EXPECT_EQ(p1.registers().get(Register::R0), 10);

    auto r2 = engine.execute(p2, 10);
    EXPECT_EQ(r2.reason, StopReason::ProcessExited);
    EXPECT_EQ(r2.pid, 2);
    EXPECT_EQ(p2.registers().get(Register::R0), 20);
}
