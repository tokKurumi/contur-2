/// @file test_register_file.cpp
/// @brief Unit tests for RegisterFile.

#include <gtest/gtest.h>

#include "contur/arch/register_file.h"

using namespace contur;

TEST(RegisterFileTest, InitialValuesAreZero)
{
    RegisterFile rf;
    for (std::uint8_t i = 0; i < REGISTER_COUNT; ++i)
    {
        EXPECT_EQ(rf.get(i), 0) << "Register " << static_cast<int>(i) << " should be 0";
    }
}

TEST(RegisterFileTest, SetAndGetByEnum)
{
    RegisterFile rf;
    rf.set(Register::R0, 100);
    rf.set(Register::R5, -42);
    rf.set(Register::R13, 999);

    EXPECT_EQ(rf.get(Register::R0), 100);
    EXPECT_EQ(rf.get(Register::R5), -42);
    EXPECT_EQ(rf.get(Register::R13), 999);
}

TEST(RegisterFileTest, SetAndGetByIndex)
{
    RegisterFile rf;
    rf.set(static_cast<std::uint8_t>(3), 77);
    EXPECT_EQ(rf.get(static_cast<std::uint8_t>(3)), 77);
}

TEST(RegisterFileTest, ProgramCounterAccessors)
{
    RegisterFile rf;
    EXPECT_EQ(rf.pc(), 0);

    rf.setPc(256);
    EXPECT_EQ(rf.pc(), 256);
    EXPECT_EQ(rf.get(Register::ProgramCounter), 256);
}

TEST(RegisterFileTest, StackPointerAccessors)
{
    RegisterFile rf;
    EXPECT_EQ(rf.sp(), 0);

    rf.setSp(1024);
    EXPECT_EQ(rf.sp(), 1024);
    EXPECT_EQ(rf.get(Register::StackPointer), 1024);
}

TEST(RegisterFileTest, Reset)
{
    RegisterFile rf;
    rf.set(Register::R0, 1);
    rf.set(Register::R7, 2);
    rf.setPc(100);
    rf.setSp(200);

    rf.reset();

    for (std::uint8_t i = 0; i < REGISTER_COUNT; ++i)
    {
        EXPECT_EQ(rf.get(i), 0) << "Register " << static_cast<int>(i) << " should be 0 after reset";
    }
}

TEST(RegisterFileTest, SnapshotAndRestore)
{
    RegisterFile rf;
    rf.set(Register::R0, 10);
    rf.set(Register::R1, 20);
    rf.set(Register::R2, 30);
    rf.setPc(100);
    rf.setSp(512);

    auto snap = rf.snapshot();

    // Modify registers after snapshot
    rf.reset();
    EXPECT_EQ(rf.get(Register::R0), 0);

    // Restore from snapshot
    rf.restore(snap);
    EXPECT_EQ(rf.get(Register::R0), 10);
    EXPECT_EQ(rf.get(Register::R1), 20);
    EXPECT_EQ(rf.get(Register::R2), 30);
    EXPECT_EQ(rf.pc(), 100);
    EXPECT_EQ(rf.sp(), 512);
}

TEST(RegisterFileTest, MoveConstruction)
{
    RegisterFile rf;
    rf.set(Register::R3, 42);
    rf.setPc(7);

    RegisterFile moved(std::move(rf));
    EXPECT_EQ(moved.get(Register::R3), 42);
    EXPECT_EQ(moved.pc(), 7);
}

TEST(RegisterFileTest, MoveAssignment)
{
    RegisterFile rf;
    rf.set(Register::R4, 55);

    RegisterFile other;
    other = std::move(rf);
    EXPECT_EQ(other.get(Register::R4), 55);
}

TEST(RegisterFileTest, DumpNotEmpty)
{
    RegisterFile rf;
    rf.set(Register::R0, 1);
    auto dumpStr = rf.dump();
    EXPECT_FALSE(dumpStr.empty());
    // Should contain register names
    EXPECT_NE(dumpStr.find("R0"), std::string::npos);
    EXPECT_NE(dumpStr.find("PC"), std::string::npos);
    EXPECT_NE(dumpStr.find("SP"), std::string::npos);
}

TEST(RegisterFileTest, NegativeValues)
{
    RegisterFile rf;
    rf.set(Register::R0, -1);
    rf.set(Register::R1, -2147483648); // INT32_MIN

    EXPECT_EQ(rf.get(Register::R0), -1);
    EXPECT_EQ(rf.get(Register::R1), -2147483648);
}

// --- Register name helper ---

TEST(RegisterNameTest, AllRegistersHaveNames)
{
    EXPECT_EQ(registerName(Register::R0), "R0");
    EXPECT_EQ(registerName(Register::R13), "R13");
    EXPECT_EQ(registerName(Register::ProgramCounter), "PC");
    EXPECT_EQ(registerName(Register::StackPointer), "SP");
}
