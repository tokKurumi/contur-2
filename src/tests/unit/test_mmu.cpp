/// @file test_mmu.cpp
/// @brief Unit tests for Mmu.

#include <gtest/gtest.h>

#include "contur/core/clock.h"

#include "contur/memory/fifo_replacement.h"
#include "contur/memory/mmu.h"
#include "contur/memory/physical_memory.h"
#include "contur/tracing/null_tracer.h"

using namespace contur;

class MmuTest : public ::testing::Test
{
    protected:
    static constexpr std::size_t MEM_SIZE = 16;

    SimulationClock clock{};
    NullTracer tracer{clock};
    PhysicalMemory memory{MEM_SIZE};

    std::unique_ptr<Mmu> createMmu()
    {
        return std::make_unique<Mmu>(memory, std::make_unique<FifoReplacement>(), tracer);
    }
};

TEST_F(MmuTest, TotalAndFreeFrames)
{
    auto mmu = createMmu();
    EXPECT_EQ(mmu->totalFrames(), MEM_SIZE);
    EXPECT_EQ(mmu->freeFrames(), MEM_SIZE);
}

TEST_F(MmuTest, AllocateReducesFreeFrames)
{
    auto mmu = createMmu();
    auto result = mmu->allocate(1, 4);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(mmu->freeFrames(), MEM_SIZE - 4);
}

TEST_F(MmuTest, AllocateAndWriteRead)
{
    auto mmu = createMmu();
    auto allocResult = mmu->allocate(1, 4);
    ASSERT_TRUE(allocResult.isOk());

    Block b{Instruction::Mov, 1, 42, 0};
    auto writeResult = mmu->write(1, 0, b);
    ASSERT_TRUE(writeResult.isOk());

    auto readResult = mmu->read(1, 0);
    ASSERT_TRUE(readResult.isOk());
    EXPECT_EQ(readResult.value().code, Instruction::Mov);
    EXPECT_EQ(readResult.value().operand, 42);
}

TEST_F(MmuTest, ReadFromInvalidProcess)
{
    auto mmu = createMmu();
    auto result = mmu->read(999, 0);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidPid);
}

TEST_F(MmuTest, WriteToInvalidProcess)
{
    auto mmu = createMmu();
    Block b{Instruction::Nop, 0, 0, 0};
    auto result = mmu->write(999, 0, b);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidPid);
}

TEST_F(MmuTest, DeallocateFreesFrames)
{
    auto mmu = createMmu();
    ASSERT_TRUE(mmu->allocate(1, 4).isOk());
    EXPECT_EQ(mmu->freeFrames(), MEM_SIZE - 4);

    auto result = mmu->deallocate(1);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(mmu->freeFrames(), MEM_SIZE);
}

TEST_F(MmuTest, DeallocateInvalidProcess)
{
    auto mmu = createMmu();
    auto result = mmu->deallocate(999);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidPid);
}

TEST_F(MmuTest, AllocateMultipleProcesses)
{
    auto mmu = createMmu();

    auto r1 = mmu->allocate(1, 4);
    ASSERT_TRUE(r1.isOk());

    auto r2 = mmu->allocate(2, 4);
    ASSERT_TRUE(r2.isOk());

    EXPECT_EQ(mmu->freeFrames(), MEM_SIZE - 8);

    // Write to process 1, read from process 2 — should be isolated
    ASSERT_TRUE(mmu->write(1, 0, Block{Instruction::Mov, 1, 100, 0}).isOk());
    ASSERT_TRUE(mmu->write(2, 0, Block{Instruction::Add, 2, 200, 0}).isOk());

    auto read1 = mmu->read(1, 0);
    ASSERT_TRUE(read1.isOk());
    EXPECT_EQ(read1.value().operand, 100);

    auto read2 = mmu->read(2, 0);
    ASSERT_TRUE(read2.isOk());
    EXPECT_EQ(read2.value().operand, 200);
}

TEST_F(MmuTest, AllocateZeroPagesIsError)
{
    auto mmu = createMmu();
    auto result = mmu->allocate(1, 0);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidAddress);
}

TEST_F(MmuTest, DoubleAllocateIsError)
{
    auto mmu = createMmu();
    auto r1 = mmu->allocate(1, 4);
    ASSERT_TRUE(r1.isOk());

    auto r2 = mmu->allocate(1, 2);
    ASSERT_TRUE(r2.isError());
    EXPECT_EQ(r2.errorCode(), ErrorCode::InvalidPid);
}

TEST_F(MmuTest, SwapOutAndSwapIn)
{
    auto mmu = createMmu();
    auto allocResult = mmu->allocate(1, 4);
    ASSERT_TRUE(allocResult.isOk());

    // Write data
    Block b{Instruction::Mul, 3, 77, 0};
    ASSERT_TRUE(mmu->write(1, 0, b).isOk());

    // Swap out page 0
    auto swapOutResult = mmu->swapOut(1, 0);
    ASSERT_TRUE(swapOutResult.isOk());

    // Reading swapped-out page should fail (not present)
    auto readResult = mmu->read(1, 0);
    ASSERT_TRUE(readResult.isError());

    // Swap back in
    auto swapInResult = mmu->swapIn(1, 0);
    ASSERT_TRUE(swapInResult.isOk());

    // Data should be restored
    readResult = mmu->read(1, 0);
    ASSERT_TRUE(readResult.isOk());
    EXPECT_EQ(readResult.value().code, Instruction::Mul);
    EXPECT_EQ(readResult.value().operand, 77);
}

TEST_F(MmuTest, SwapInAlreadyPresentIsNoOp)
{
    auto mmu = createMmu();
    ASSERT_TRUE(mmu->allocate(1, 2).isOk());
    auto result = mmu->swapIn(1, 0);
    ASSERT_TRUE(result.isOk());
}

TEST_F(MmuTest, SwapOutAlreadySwappedIsNoOp)
{
    auto mmu = createMmu();
    ASSERT_TRUE(mmu->allocate(1, 2).isOk());
    ASSERT_TRUE(mmu->swapOut(1, 0).isOk());
    auto result = mmu->swapOut(1, 0);
    ASSERT_TRUE(result.isOk());
}

TEST_F(MmuTest, MoveSemantics)
{
    auto mmu1 = createMmu();
    ASSERT_TRUE(mmu1->allocate(1, 4).isOk());
    ASSERT_TRUE(mmu1->write(1, 0, Block{Instruction::Mov, 0, 99, 0}).isOk());

    Mmu mmu2 = std::move(*mmu1);
    auto readResult = mmu2.read(1, 0);
    ASSERT_TRUE(readResult.isOk());
    EXPECT_EQ(readResult.value().operand, 99);
}
