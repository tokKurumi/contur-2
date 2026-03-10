/// @file test_cpu.cpp
/// @brief Unit tests for Cpu — fetch-decode-execute cycle.

#include <gtest/gtest.h>

#include "contur/arch/block.h"
#include "contur/arch/instruction.h"
#include "contur/arch/interrupt.h"
#include "contur/arch/register_file.h"
#include "contur/cpu/alu.h"
#include "contur/cpu/cpu.h"
#include "contur/memory/physical_memory.h"

using namespace contur;

/// @brief Test fixture that sets up a CPU connected to 256-cell physical memory.
class CpuTest : public ::testing::Test
{
    protected:
    PhysicalMemory memory{256};
    Cpu cpu{memory};
    RegisterFile regs;

    /// @brief Loads a program (vector of Blocks) into memory starting at the given address.
    void loadProgram(const std::vector<Block> &program, MemoryAddress startAddr = 0)
    {
        for (std::size_t i = 0; i < program.size(); ++i)
        {
            ASSERT_TRUE(memory.write(startAddr + static_cast<MemoryAddress>(i), program[i]).isOk());
        }
        regs.setPc(startAddr);
    }
};

// Nop

TEST_F(CpuTest, NopAdvancesPC)
{
    loadProgram({Block{Instruction::Nop, 0, 0}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.pc(), 1);
}

// Mov

TEST_F(CpuTest, MovImmediate)
{
    loadProgram({Block{Instruction::Mov, 1, 42}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.get(Register::R1), 42);
}

TEST_F(CpuTest, MovRegisterToRegister)
{
    regs.set(Register::R3, 99);
    // state=1 means operand is a register index
    loadProgram({Block{Instruction::Mov, 1, 3, 1}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.get(Register::R1), 99);
}

// Arithmetic (through ALU)

TEST_F(CpuTest, AddImmediate)
{
    regs.set(Register::R0, 10);
    loadProgram({Block{Instruction::Add, 0, 5}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.get(Register::R0), 15);
}

TEST_F(CpuTest, AddRegisterMode)
{
    regs.set(Register::R0, 10);
    regs.set(Register::R1, 20);
    loadProgram({Block{Instruction::Add, 0, 1, 1}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.get(Register::R0), 30);
}

TEST_F(CpuTest, SubImmediate)
{
    regs.set(Register::R0, 20);
    loadProgram({Block{Instruction::Sub, 0, 7}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.get(Register::R0), 13);
}

TEST_F(CpuTest, MulImmediate)
{
    regs.set(Register::R0, 6);
    loadProgram({Block{Instruction::Mul, 0, 7}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.get(Register::R0), 42);
}

TEST_F(CpuTest, DivImmediate)
{
    regs.set(Register::R0, 42);
    loadProgram({Block{Instruction::Div, 0, 6}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.get(Register::R0), 7);
}

TEST_F(CpuTest, DivByZeroReturnsDivByZero)
{
    regs.set(Register::R0, 42);
    loadProgram({Block{Instruction::Div, 0, 0}});
    EXPECT_EQ(cpu.step(regs), Interrupt::DivByZero);
}

// Bitwise operations

TEST_F(CpuTest, AndImmediate)
{
    regs.set(Register::R0, 0b1100);
    loadProgram({Block{Instruction::And, 0, 0b1010}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.get(Register::R0), 0b1000);
}

TEST_F(CpuTest, OrImmediate)
{
    regs.set(Register::R0, 0b1100);
    loadProgram({Block{Instruction::Or, 0, 0b0011}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.get(Register::R0), 0b1111);
}

TEST_F(CpuTest, XorImmediate)
{
    regs.set(Register::R0, 0b1100);
    loadProgram({Block{Instruction::Xor, 0, 0b1010}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.get(Register::R0), 0b0110);
}

TEST_F(CpuTest, ShiftLeftImmediate)
{
    regs.set(Register::R0, 1);
    loadProgram({Block{Instruction::ShiftLeft, 0, 3}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.get(Register::R0), 8);
}

TEST_F(CpuTest, ShiftRightImmediate)
{
    regs.set(Register::R0, 16);
    loadProgram({Block{Instruction::ShiftRight, 0, 2}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.get(Register::R0), 4);
}

// Compare + conditional jumps

TEST_F(CpuTest, CompareSetsFlagsEqual)
{
    regs.set(Register::R0, 5);
    loadProgram({Block{Instruction::Compare, 0, 5}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_NE(cpu.flags() & ALU::ZERO_FLAG, 0);
}

TEST_F(CpuTest, CompareSetsFlagsLessThan)
{
    regs.set(Register::R0, 3);
    loadProgram({Block{Instruction::Compare, 0, 7}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_NE(cpu.flags() & ALU::SIGN_FLAG, 0);
}

TEST_F(CpuTest, JumpEqualTaken)
{
    regs.set(Register::R0, 5);
    loadProgram({
        Block{Instruction::Compare, 0, 5},    // addr 0: CMP R0, 5 → equal
        Block{Instruction::JumpEqual, 0, 10}, // addr 1: JE 10
    });
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok); // CMP
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok); // JE
    EXPECT_EQ(regs.pc(), 10);
}

TEST_F(CpuTest, JumpEqualNotTaken)
{
    regs.set(Register::R0, 3);
    loadProgram({
        Block{Instruction::Compare, 0, 5},    // addr 0: CMP R0, 5 → not equal
        Block{Instruction::JumpEqual, 0, 10}, // addr 1: JE 10 → not taken
    });
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok); // CMP
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok); // JE (not taken)
    EXPECT_EQ(regs.pc(), 2);                  // PC advanced past JE
}

TEST_F(CpuTest, JumpNotEqualTaken)
{
    regs.set(Register::R0, 3);
    loadProgram({
        Block{Instruction::Compare, 0, 5},
        Block{Instruction::JumpNotEqual, 0, 10},
    });
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.pc(), 10);
}

TEST_F(CpuTest, JumpGreaterTaken)
{
    regs.set(Register::R0, 10);
    loadProgram({
        Block{Instruction::Compare, 0, 3},
        Block{Instruction::JumpGreater, 0, 20},
    });
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.pc(), 20);
}

TEST_F(CpuTest, JumpLessTaken)
{
    regs.set(Register::R0, 1);
    loadProgram({
        Block{Instruction::Compare, 0, 10},
        Block{Instruction::JumpLess, 0, 15},
    });
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.pc(), 15);
}

TEST_F(CpuTest, JumpGreaterEqualTakenEqual)
{
    regs.set(Register::R0, 5);
    loadProgram({
        Block{Instruction::Compare, 0, 5},
        Block{Instruction::JumpGreaterEqual, 0, 25},
    });
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.pc(), 25);
}

TEST_F(CpuTest, JumpLessEqualTakenLess)
{
    regs.set(Register::R0, 2);
    loadProgram({
        Block{Instruction::Compare, 0, 5},
        Block{Instruction::JumpLessEqual, 0, 30},
    });
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.pc(), 30);
}

// Push / Pop

TEST_F(CpuTest, PushAndPop)
{
    regs.set(Register::R0, 42);
    regs.setSp(200); // Stack grows downward
    loadProgram({
        Block{Instruction::Push, 0, 0}, // PUSH R0
        Block{Instruction::Pop, 1, 0},  // POP R1
    });
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok); // Push
    EXPECT_EQ(regs.sp(), 199);

    ASSERT_EQ(cpu.step(regs), Interrupt::Ok); // Pop
    EXPECT_EQ(regs.get(Register::R1), 42);
    EXPECT_EQ(regs.sp(), 200);
}

// Call / Return

TEST_F(CpuTest, CallAndReturn)
{
    regs.setSp(200);
    loadProgram({
        Block{Instruction::Call, 0, 10}, // addr 0: CALL 10
        Block{Instruction::Nop, 0, 0},   // addr 1: (return target)
    });
    // Also place a RET at address 10
    ASSERT_TRUE(memory.write(10, Block{Instruction::Return, 0, 0}).isOk());

    ASSERT_EQ(cpu.step(regs), Interrupt::Ok); // CALL → pushes return addr (1), jumps to 10
    EXPECT_EQ(regs.pc(), 10);
    EXPECT_EQ(regs.sp(), 199);

    ASSERT_EQ(cpu.step(regs), Interrupt::Ok); // RET → pops addr (1), jumps to 1
    EXPECT_EQ(regs.pc(), 1);
    EXPECT_EQ(regs.sp(), 200);
}

// Memory read / write

TEST_F(CpuTest, WriteAndReadMemory)
{
    regs.set(Register::R0, 99);
    loadProgram({
        Block{Instruction::WriteMemory, 0, 100}, // WMEM R0 → addr 100
        Block{Instruction::ReadMemory, 1, 100},  // RMEM R1 ← addr 100
    });
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    EXPECT_EQ(regs.get(Register::R1), 99);
}

// Interrupts

TEST_F(CpuTest, InterruptExit)
{
    loadProgram({
        Block{Instruction::Interrupt, 0, static_cast<std::int32_t>(Interrupt::Exit)},
    });
    EXPECT_EQ(cpu.step(regs), Interrupt::Exit);
}

TEST_F(CpuTest, InterruptSyscall)
{
    loadProgram({
        Block{Instruction::Interrupt, 0, static_cast<std::int32_t>(Interrupt::SystemCall)},
    });
    EXPECT_EQ(cpu.step(regs), Interrupt::SystemCall);
}

TEST_F(CpuTest, InterruptDeviceIO)
{
    loadProgram({
        Block{Instruction::Interrupt, 0, static_cast<std::int32_t>(Interrupt::DeviceIO)},
    });
    EXPECT_EQ(cpu.step(regs), Interrupt::DeviceIO);
}

// Halt

TEST_F(CpuTest, HaltReturnsExit)
{
    loadProgram({Block{Instruction::Halt, 0, 0}});
    EXPECT_EQ(cpu.step(regs), Interrupt::Exit);
}

// Reset

TEST_F(CpuTest, ResetClearsFlags)
{
    regs.set(Register::R0, 5);
    loadProgram({Block{Instruction::Compare, 0, 5}});
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    ASSERT_NE(cpu.flags(), 0);

    cpu.reset();
    EXPECT_EQ(cpu.flags(), 0);
}

// Multi-instruction program

TEST_F(CpuTest, SimpleProgram)
{
    // Program: R0 = 10, R1 = 20, R0 = R0 + R1, HALT
    loadProgram({
        Block{Instruction::Mov, 0, 10},   // R0 = 10
        Block{Instruction::Mov, 1, 20},   // R1 = 20
        Block{Instruction::Add, 0, 1, 1}, // R0 = R0 + R1 (register mode)
        Block{Instruction::Halt, 0, 0},   // HALT
    });

    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    ASSERT_EQ(cpu.step(regs), Interrupt::Ok);
    ASSERT_EQ(cpu.step(regs), Interrupt::Exit);

    EXPECT_EQ(regs.get(Register::R0), 30);
    EXPECT_EQ(regs.get(Register::R1), 20);
}

TEST_F(CpuTest, CountdownLoop)
{
    // Program: R0 = 5, loop: R0 = R0 - 1, CMP R0, 0, JNE loop, HALT
    loadProgram({
        Block{Instruction::Mov, 0, 5},          // addr 0: R0 = 5
        Block{Instruction::Sub, 0, 1},          // addr 1: R0 = R0 - 1
        Block{Instruction::Compare, 0, 0},      // addr 2: CMP R0, 0
        Block{Instruction::JumpNotEqual, 0, 1}, // addr 3: JNE 1
        Block{Instruction::Halt, 0, 0},         // addr 4: HALT
    });

    // Execute: 1 MOV + 5 * (SUB + CMP + JNE) + HALT
    // = 1 + 5*3 + 1 = 17 steps (last JNE falls through)
    Interrupt result = Interrupt::Ok;
    int steps = 0;
    while (result == Interrupt::Ok)
    {
        result = cpu.step(regs);
        steps++;
        ASSERT_LT(steps, 100) << "Infinite loop detected";
    }

    EXPECT_EQ(result, Interrupt::Exit);
    EXPECT_EQ(regs.get(Register::R0), 0);
}

// Error handling

TEST_F(CpuTest, FetchFromInvalidAddressReturnsError)
{
    regs.setPc(9999); // Beyond memory size
    EXPECT_EQ(cpu.step(regs), Interrupt::Error);
}
