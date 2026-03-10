/// @file test_physical_memory.cpp
/// @brief Unit tests for PhysicalMemory.

#include "contur/memory/physical_memory.h"

#include <gtest/gtest.h>

using namespace contur;

TEST(PhysicalMemoryTest, ConstructionWithSize)
{
    PhysicalMemory mem(64);
    EXPECT_EQ(mem.size(), 64u);
}

TEST(PhysicalMemoryTest, ReadDefaultBlockIsNop)
{
    PhysicalMemory mem(8);
    auto result = mem.read(0);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value().code, Instruction::Nop);
    EXPECT_EQ(result.value().reg, 0);
    EXPECT_EQ(result.value().operand, 0);
    EXPECT_EQ(result.value().state, 0);
}

TEST(PhysicalMemoryTest, WriteAndRead)
{
    PhysicalMemory mem(8);
    Block b{Instruction::Mov, 1, 42, 0};
    auto writeResult = mem.write(3, b);
    ASSERT_TRUE(writeResult.isOk());

    auto readResult = mem.read(3);
    ASSERT_TRUE(readResult.isOk());
    EXPECT_EQ(readResult.value().code, Instruction::Mov);
    EXPECT_EQ(readResult.value().reg, 1);
    EXPECT_EQ(readResult.value().operand, 42);
}

TEST(PhysicalMemoryTest, ReadOutOfBounds)
{
    PhysicalMemory mem(4);
    auto result = mem.read(4);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidAddress);
}

TEST(PhysicalMemoryTest, WriteOutOfBounds)
{
    PhysicalMemory mem(4);
    Block b{Instruction::Nop, 0, 0, 0};
    auto result = mem.write(10, b);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidAddress);
}

TEST(PhysicalMemoryTest, Clear)
{
    PhysicalMemory mem(4);
    ASSERT_TRUE(mem.write(0, Block{Instruction::Add, 1, 2, 3}).isOk());
    ASSERT_TRUE(mem.write(1, Block{Instruction::Sub, 4, 5, 6}).isOk());
    mem.clear();

    for (MemoryAddress i = 0; i < 4; ++i) {
        auto result = mem.read(i);
        ASSERT_TRUE(result.isOk());
        EXPECT_EQ(result.value().code, Instruction::Nop);
    }
}

TEST(PhysicalMemoryTest, WriteRangeAndReadRange)
{
    PhysicalMemory mem(8);
    std::vector<Block> blocks = {
        {Instruction::Mov, 0, 10, 0},
        {Instruction::Add, 1, 20, 0},
        {Instruction::Sub, 2, 30, 0},
    };

    auto writeResult = mem.writeRange(2, blocks);
    ASSERT_TRUE(writeResult.isOk());

    auto readResult = mem.readRange(2, 3);
    ASSERT_TRUE(readResult.isOk());
    ASSERT_EQ(readResult.value().size(), 3u);
    EXPECT_EQ(readResult.value()[0].code, Instruction::Mov);
    EXPECT_EQ(readResult.value()[1].code, Instruction::Add);
    EXPECT_EQ(readResult.value()[2].code, Instruction::Sub);
    EXPECT_EQ(readResult.value()[0].operand, 10);
    EXPECT_EQ(readResult.value()[1].operand, 20);
    EXPECT_EQ(readResult.value()[2].operand, 30);
}

TEST(PhysicalMemoryTest, WriteRangeOutOfBounds)
{
    PhysicalMemory mem(4);
    std::vector<Block> blocks = {
        {Instruction::Nop, 0, 0, 0},
        {Instruction::Nop, 0, 0, 0},
        {Instruction::Nop, 0, 0, 0},
    };

    auto result = mem.writeRange(3, blocks);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidAddress);
}

TEST(PhysicalMemoryTest, ReadRangeOutOfBounds)
{
    PhysicalMemory mem(4);
    auto result = mem.readRange(2, 5);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidAddress);
}

TEST(PhysicalMemoryTest, ClearRange)
{
    PhysicalMemory mem(8);
    ASSERT_TRUE(mem.write(2, Block{Instruction::Mov, 1, 42, 0}).isOk());
    ASSERT_TRUE(mem.write(3, Block{Instruction::Add, 2, 10, 0}).isOk());
    ASSERT_TRUE(mem.write(4, Block{Instruction::Sub, 3, 5, 0}).isOk());

    auto result = mem.clearRange(2, 3);
    ASSERT_TRUE(result.isOk());

    for (MemoryAddress i = 2; i < 5; ++i) {
        auto readResult = mem.read(i);
        ASSERT_TRUE(readResult.isOk());
        EXPECT_EQ(readResult.value().code, Instruction::Nop);
    }
}

TEST(PhysicalMemoryTest, ClearRangeOutOfBounds)
{
    PhysicalMemory mem(4);
    auto result = mem.clearRange(2, 5);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidAddress);
}

TEST(PhysicalMemoryTest, MoveSemantics)
{
    PhysicalMemory mem1(8);
    ASSERT_TRUE(mem1.write(0, Block{Instruction::Mov, 1, 42, 0}).isOk());

    PhysicalMemory mem2 = std::move(mem1);
    EXPECT_EQ(mem2.size(), 8u);

    auto result = mem2.read(0);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value().operand, 42);
}
