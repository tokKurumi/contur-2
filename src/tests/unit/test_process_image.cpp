/// @file test_process_image.cpp
/// @brief Unit tests for ProcessImage.

#include <vector>

#include <gtest/gtest.h>

#include "contur/arch/block.h"
#include "contur/arch/instruction.h"
#include "contur/arch/register_file.h"
#include "contur/process/priority.h"
#include "contur/process/process_image.h"
#include "contur/process/state.h"

using namespace contur;

// Helper: create a simple 3-instruction program
static std::vector<Block> makeSimpleProgram()
{
    return {
        Block{Instruction::Mov, 1, 42},
        Block{Instruction::Add, 1, 10},
        Block{Instruction::Halt, 0, 0},
    };
}

// Construction

TEST(ProcessImageTest, ConstructWithDefaults)
{
    ProcessImage img(1, "test", makeSimpleProgram());
    EXPECT_EQ(img.id(), 1u);
    EXPECT_EQ(img.name(), "test");
    EXPECT_EQ(img.state(), ProcessState::New);
    EXPECT_EQ(img.priority().base, PriorityLevel::Normal);
    EXPECT_EQ(img.codeSize(), 3u);
}

TEST(ProcessImageTest, ConstructWithPriority)
{
    ProcessImage img(2, "high", makeSimpleProgram(), Priority(PriorityLevel::High));
    EXPECT_EQ(img.priority().base, PriorityLevel::High);
    EXPECT_EQ(img.priority().effective, PriorityLevel::High);
}

TEST(ProcessImageTest, ConstructWithArrivalTime)
{
    ProcessImage img(3, "timed", makeSimpleProgram(), Priority{}, 100);
    EXPECT_EQ(img.pcb().timing().arrivalTime, 100u);
}

TEST(ProcessImageTest, EmptyCodeSegment)
{
    ProcessImage img(4, "empty", {});
    EXPECT_EQ(img.codeSize(), 0u);
    EXPECT_TRUE(img.code().empty());
}

// Move semantics

TEST(ProcessImageTest, MoveConstruction)
{
    ProcessImage original(1, "mover", makeSimpleProgram(), Priority(PriorityLevel::Low));
    ProcessImage moved(std::move(original));
    EXPECT_EQ(moved.id(), 1u);
    EXPECT_EQ(moved.name(), "mover");
    EXPECT_EQ(moved.codeSize(), 3u);
    EXPECT_EQ(moved.priority().base, PriorityLevel::Low);
}

TEST(ProcessImageTest, MoveAssignment)
{
    ProcessImage a(1, "alpha", makeSimpleProgram());
    ProcessImage b(2, "beta", {});
    b = std::move(a);
    EXPECT_EQ(b.id(), 1u);
    EXPECT_EQ(b.name(), "alpha");
    EXPECT_EQ(b.codeSize(), 3u);
}

// PCB access

TEST(ProcessImageTest, PcbConstAccess)
{
    const ProcessImage img(1, "const", makeSimpleProgram());
    EXPECT_EQ(img.pcb().id(), 1u);
    EXPECT_EQ(img.pcb().name(), "const");
    EXPECT_EQ(img.pcb().state(), ProcessState::New);
}

TEST(ProcessImageTest, PcbMutableAccess)
{
    ProcessImage img(1, "mutable", makeSimpleProgram());
    EXPECT_TRUE(img.pcb().setState(ProcessState::Ready, 1));
    EXPECT_EQ(img.state(), ProcessState::Ready);
}

TEST(ProcessImageTest, PcbStateTransitionThroughImage)
{
    ProcessImage img(1, "transition", makeSimpleProgram());
    EXPECT_TRUE(img.pcb().setState(ProcessState::Ready, 1));
    EXPECT_TRUE(img.pcb().setState(ProcessState::Running, 2));
    EXPECT_EQ(img.state(), ProcessState::Running);
}

// Register file access

TEST(ProcessImageTest, RegistersInitiallyZero)
{
    ProcessImage img(1, "regs", makeSimpleProgram());
    EXPECT_EQ(img.registers().get(Register::R0), 0);
    EXPECT_EQ(img.registers().pc(), 0);
    EXPECT_EQ(img.registers().sp(), 0);
}

TEST(ProcessImageTest, RegistersWritable)
{
    ProcessImage img(1, "regs", makeSimpleProgram());
    img.registers().set(Register::R1, 42);
    img.registers().setPc(100);
    EXPECT_EQ(img.registers().get(Register::R1), 42);
    EXPECT_EQ(img.registers().pc(), 100);
}

TEST(ProcessImageTest, ConstRegistersReadOnly)
{
    const ProcessImage img(1, "const_regs", makeSimpleProgram());
    EXPECT_EQ(img.registers().get(Register::R0), 0);
    EXPECT_EQ(img.registers().pc(), 0);
}

TEST(ProcessImageTest, RegisterSnapshotRestore)
{
    ProcessImage img(1, "snap", makeSimpleProgram());
    img.registers().set(Register::R0, 10);
    img.registers().set(Register::R5, 50);
    img.registers().setPc(200);

    auto snapshot = img.registers().snapshot();

    img.registers().reset();
    EXPECT_EQ(img.registers().get(Register::R0), 0);

    img.registers().restore(snapshot);
    EXPECT_EQ(img.registers().get(Register::R0), 10);
    EXPECT_EQ(img.registers().get(Register::R5), 50);
    EXPECT_EQ(img.registers().pc(), 200);
}

// Code segment

TEST(ProcessImageTest, ReadCodeByOffset)
{
    ProcessImage img(1, "code", makeSimpleProgram());
    EXPECT_EQ(img.readCode(0).code, Instruction::Mov);
    EXPECT_EQ(img.readCode(0).reg, 1);
    EXPECT_EQ(img.readCode(0).operand, 42);
    EXPECT_EQ(img.readCode(1).code, Instruction::Add);
    EXPECT_EQ(img.readCode(2).code, Instruction::Halt);
}

TEST(ProcessImageTest, CodeReturnsFullVector)
{
    auto program = makeSimpleProgram();
    ProcessImage img(1, "full", program);
    const auto &code = img.code();
    ASSERT_EQ(code.size(), program.size());
    for (std::size_t i = 0; i < program.size(); ++i)
    {
        EXPECT_EQ(code[i], program[i]);
    }
}

TEST(ProcessImageTest, SetCodeReplacesProgram)
{
    ProcessImage img(1, "replace", makeSimpleProgram());
    ASSERT_EQ(img.codeSize(), 3u);

    std::vector<Block> newCode = {
        Block{Instruction::Nop, 0, 0},
        Block{Instruction::Halt, 0, 0},
    };
    img.setCode(std::move(newCode));
    EXPECT_EQ(img.codeSize(), 2u);
    EXPECT_EQ(img.readCode(0).code, Instruction::Nop);
    EXPECT_EQ(img.readCode(1).code, Instruction::Halt);
}

TEST(ProcessImageTest, SetCodeToEmpty)
{
    ProcessImage img(1, "clear", makeSimpleProgram());
    img.setCode({});
    EXPECT_EQ(img.codeSize(), 0u);
    EXPECT_TRUE(img.code().empty());
}

// Convenience accessors

TEST(ProcessImageTest, ConvenienceIdMatchesPcb)
{
    ProcessImage img(42, "conv", makeSimpleProgram());
    EXPECT_EQ(img.id(), img.pcb().id());
}

TEST(ProcessImageTest, ConvenienceNameMatchesPcb)
{
    ProcessImage img(1, "convenience", makeSimpleProgram());
    EXPECT_EQ(img.name(), img.pcb().name());
}

TEST(ProcessImageTest, ConvenienceStateMatchesPcb)
{
    ProcessImage img(1, "state", makeSimpleProgram());
    EXPECT_EQ(img.state(), img.pcb().state());
    ASSERT_TRUE(img.pcb().setState(ProcessState::Ready, 1));
    EXPECT_EQ(img.state(), img.pcb().state());
}

TEST(ProcessImageTest, ConveniencePriorityMatchesPcb)
{
    ProcessImage img(1, "prio", makeSimpleProgram(), Priority(PriorityLevel::High));
    EXPECT_EQ(img.priority().base, img.pcb().priority().base);
    EXPECT_EQ(img.priority().effective, img.pcb().priority().effective);
}

// Integration: process lifecycle with code execution state

TEST(ProcessImageTest, FullLifecycleWithState)
{
    ProcessImage img(1, "lifecycle", makeSimpleProgram(), Priority(PriorityLevel::Normal), 0);

    // Simulate lifecycle
    ASSERT_TRUE(img.pcb().setState(ProcessState::Ready, 1));
    ASSERT_TRUE(img.pcb().setState(ProcessState::Running, 3));

    // Simulate execution: advance PC
    img.registers().setPc(0);
    for (std::size_t i = 0; i < img.codeSize(); ++i)
    {
        const auto &instr = img.readCode(static_cast<std::size_t>(img.registers().pc()));
        img.registers().setPc(img.registers().pc() + 1);
        if (instr.code == Instruction::Halt)
        {
            break;
        }
    }
    EXPECT_EQ(img.registers().pc(), 3); // PC at end of program

    // Terminate
    ASSERT_TRUE(img.pcb().setState(ProcessState::Terminated, 6));
    EXPECT_EQ(img.state(), ProcessState::Terminated);
    EXPECT_EQ(img.pcb().timing().totalCpuTime, 3u); // ticks 3-6
}
