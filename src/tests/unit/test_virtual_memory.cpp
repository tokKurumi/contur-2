/// @file test_virtual_memory.cpp
/// @brief Unit tests for VirtualMemory.

#include <gtest/gtest.h>

#include "contur/memory/fifo_replacement.h"
#include "contur/memory/mmu.h"
#include "contur/memory/physical_memory.h"
#include "contur/memory/virtual_memory.h"

using namespace contur;

class VirtualMemoryTest : public ::testing::Test
{
    protected:
    static constexpr std::size_t MEM_SIZE = 32;
    static constexpr std::size_t MAX_SLOTS = 8;

    PhysicalMemory memory{MEM_SIZE};
    Mmu mmu{memory, std::make_unique<FifoReplacement>()};
    VirtualMemory vm{mmu, MAX_SLOTS};
};

TEST_F(VirtualMemoryTest, InitialState)
{
    EXPECT_EQ(vm.totalSlots(), MAX_SLOTS);
    EXPECT_EQ(vm.freeSlots(), MAX_SLOTS);
}

TEST_F(VirtualMemoryTest, AllocateSlot)
{
    auto result = vm.allocateSlot(1, 4);
    ASSERT_TRUE(result.isOk());
    EXPECT_TRUE(vm.hasSlot(1));
    EXPECT_EQ(vm.slotSize(1), 4u);
    EXPECT_EQ(vm.freeSlots(), MAX_SLOTS - 1);
}

TEST_F(VirtualMemoryTest, FreeSlot)
{
    ASSERT_TRUE(vm.allocateSlot(1, 4).isOk());
    auto result = vm.freeSlot(1);
    ASSERT_TRUE(result.isOk());
    EXPECT_FALSE(vm.hasSlot(1));
    EXPECT_EQ(vm.freeSlots(), MAX_SLOTS);
}

TEST_F(VirtualMemoryTest, FreeSlotInvalidProcess)
{
    auto result = vm.freeSlot(999);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidPid);
}

TEST_F(VirtualMemoryTest, AllocateSlotZeroSizeIsError)
{
    auto result = vm.allocateSlot(1, 0);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidAddress);
}

TEST_F(VirtualMemoryTest, AllocateDuplicateIsError)
{
    ASSERT_TRUE(vm.allocateSlot(1, 4).isOk());
    auto result = vm.allocateSlot(1, 2);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidPid);
}

TEST_F(VirtualMemoryTest, LoadAndReadSegment)
{
    ASSERT_TRUE(vm.allocateSlot(1, 4).isOk());

    std::vector<Block> data = {
        {Instruction::Mov, 0, 10, 0},
        {Instruction::Add, 1, 20, 0},
        {Instruction::Sub, 2, 30, 0},
        {Instruction::Halt, 0, 0, 0},
    };

    auto loadResult = vm.loadSegment(1, data);
    ASSERT_TRUE(loadResult.isOk());

    auto readResult = vm.readSegment(1);
    ASSERT_TRUE(readResult.isOk());
    ASSERT_EQ(readResult.value().size(), 4u);
    EXPECT_EQ(readResult.value()[0].code, Instruction::Mov);
    EXPECT_EQ(readResult.value()[0].operand, 10);
    EXPECT_EQ(readResult.value()[1].code, Instruction::Add);
    EXPECT_EQ(readResult.value()[1].operand, 20);
    EXPECT_EQ(readResult.value()[2].code, Instruction::Sub);
    EXPECT_EQ(readResult.value()[2].operand, 30);
    EXPECT_EQ(readResult.value()[3].code, Instruction::Halt);
}

TEST_F(VirtualMemoryTest, LoadSegmentTooLargeIsError)
{
    ASSERT_TRUE(vm.allocateSlot(1, 2).isOk());

    std::vector<Block> data = {
        {Instruction::Nop, 0, 0, 0},
        {Instruction::Nop, 0, 0, 0},
        {Instruction::Nop, 0, 0, 0},
    };

    auto result = vm.loadSegment(1, data);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::OutOfMemory);
}

TEST_F(VirtualMemoryTest, LoadSegmentInvalidProcessIsError)
{
    auto result = vm.loadSegment(999, {{Instruction::Nop, 0, 0, 0}});
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidPid);
}

TEST_F(VirtualMemoryTest, ReadSegmentInvalidProcessIsError)
{
    auto result = vm.readSegment(999);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidPid);
}

TEST_F(VirtualMemoryTest, HasSlotReturnsFalseForUnknownProcess)
{
    EXPECT_FALSE(vm.hasSlot(42));
}

TEST_F(VirtualMemoryTest, SlotSizeReturnsZeroForUnknownProcess)
{
    EXPECT_EQ(vm.slotSize(42), 0u);
}

TEST_F(VirtualMemoryTest, MultipleProcesses)
{
    ASSERT_TRUE(vm.allocateSlot(1, 4).isOk());
    ASSERT_TRUE(vm.allocateSlot(2, 3).isOk());

    EXPECT_TRUE(vm.hasSlot(1));
    EXPECT_TRUE(vm.hasSlot(2));
    EXPECT_EQ(vm.slotSize(1), 4u);
    EXPECT_EQ(vm.slotSize(2), 3u);
    EXPECT_EQ(vm.freeSlots(), MAX_SLOTS - 2);

    // Load different data to each process
    ASSERT_TRUE(vm.loadSegment(
                      1,
                      {{Instruction::Mov, 0, 100, 0},
                       {Instruction::Halt, 0, 0, 0},
                       {Instruction::Nop, 0, 0, 0},
                       {Instruction::Nop, 0, 0, 0}}
    )
                    .isOk());
    ASSERT_TRUE(
        vm.loadSegment(2, {{Instruction::Add, 1, 200, 0}, {Instruction::Halt, 0, 0, 0}, {Instruction::Nop, 0, 0, 0}})
            .isOk()
    );

    auto seg1 = vm.readSegment(1);
    ASSERT_TRUE(seg1.isOk());
    EXPECT_EQ(seg1.value()[0].operand, 100);

    auto seg2 = vm.readSegment(2);
    ASSERT_TRUE(seg2.isOk());
    EXPECT_EQ(seg2.value()[0].operand, 200);
}

TEST_F(VirtualMemoryTest, AllocateBeyondMaxSlotsIsError)
{
    for (ProcessId pid = 1; pid <= MAX_SLOTS; ++pid)
    {
        auto result = vm.allocateSlot(pid, 2);
        ASSERT_TRUE(result.isOk()) << "Failed to allocate slot for pid " << pid;
    }

    auto result = vm.allocateSlot(MAX_SLOTS + 1, 2);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::OutOfMemory);
}

TEST_F(VirtualMemoryTest, MoveSemantics)
{
    ASSERT_TRUE(vm.allocateSlot(1, 4).isOk());
    ASSERT_TRUE(vm.loadSegment(
                      1,
                      {{Instruction::Mov, 0, 55, 0},
                       {Instruction::Nop, 0, 0, 0},
                       {Instruction::Nop, 0, 0, 0},
                       {Instruction::Nop, 0, 0, 0}}
    )
                    .isOk());

    VirtualMemory vm2 = std::move(vm);
    EXPECT_TRUE(vm2.hasSlot(1));

    auto seg = vm2.readSegment(1);
    ASSERT_TRUE(seg.isOk());
    EXPECT_EQ(seg.value()[0].operand, 55);
}
