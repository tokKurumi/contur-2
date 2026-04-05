/// @file test_virtual_memory_extended.cpp
/// @brief Extended unit tests for VirtualMemory — slot reuse after free,
///        double-free, totalSlots invariant, segment exact-fit, segment reload.

#include <gtest/gtest.h>

#include "contur/core/clock.h"

#include "contur/arch/instruction.h"
#include "contur/memory/fifo_replacement.h"
#include "contur/memory/mmu.h"
#include "contur/memory/physical_memory.h"
#include "contur/memory/virtual_memory.h"
#include "contur/tracing/null_tracer.h"

using namespace contur;

namespace {

    struct VmFix
    {
        SimulationClock clock;
        NullTracer tracer{clock};
        PhysicalMemory memory{64};
        Mmu mmu{memory, std::make_unique<FifoReplacement>(), tracer};
        VirtualMemory vm{mmu, 6};
    };

} // namespace

// totalSlots invariant
TEST(VirtualMemoryExtTest, TotalSlotsNeverChanges)
{
    VmFix f;
    const std::size_t total = f.vm.totalSlots();

    ASSERT_TRUE(f.vm.allocateSlot(1, 2).isOk());
    EXPECT_EQ(f.vm.totalSlots(), total);

    ASSERT_TRUE(f.vm.freeSlot(1).isOk());
    EXPECT_EQ(f.vm.totalSlots(), total);
}

// Slot reuse after free
TEST(VirtualMemoryExtTest, SlotReusableAfterFree)
{
    VmFix f;
    ASSERT_TRUE(f.vm.allocateSlot(1, 3).isOk());
    ASSERT_TRUE(f.vm.freeSlot(1).isOk());
    EXPECT_FALSE(f.vm.hasSlot(1));

    // Reallocate the same PID
    ASSERT_TRUE(f.vm.allocateSlot(1, 2).isOk());
    EXPECT_TRUE(f.vm.hasSlot(1));
    EXPECT_EQ(f.vm.slotSize(1), 2u);
}

TEST(VirtualMemoryExtTest, FreeSlotRestoresFreeCount)
{
    VmFix f;
    const std::size_t initial = f.vm.freeSlots();

    ASSERT_TRUE(f.vm.allocateSlot(10, 2).isOk());
    ASSERT_TRUE(f.vm.allocateSlot(11, 2).isOk());
    EXPECT_EQ(f.vm.freeSlots(), initial - 2);

    ASSERT_TRUE(f.vm.freeSlot(10).isOk());
    EXPECT_EQ(f.vm.freeSlots(), initial - 1);

    ASSERT_TRUE(f.vm.freeSlot(11).isOk());
    EXPECT_EQ(f.vm.freeSlots(), initial);
}

// Double-free
TEST(VirtualMemoryExtTest, DoubleFreeReturnsError)
{
    VmFix f;
    ASSERT_TRUE(f.vm.allocateSlot(1, 2).isOk());
    ASSERT_TRUE(f.vm.freeSlot(1).isOk());

    auto r = f.vm.freeSlot(1);
    ASSERT_TRUE(r.isError());
    EXPECT_EQ(r.errorCode(), ErrorCode::InvalidPid);
}

// freeSlots decrements and recovers with many processes
TEST(VirtualMemoryExtTest, FreeSlotsDecrementsMontionicallyWithAllocations)
{
    VmFix f;
    const std::size_t cap = f.vm.totalSlots();

    for (ProcessId pid = 1; pid <= cap; ++pid)
    {
        ASSERT_TRUE(f.vm.allocateSlot(pid, 1).isOk()) << "alloc pid=" << pid;
        EXPECT_EQ(f.vm.freeSlots(), cap - pid) << "after allocating pid=" << pid;
    }
}

// Segment: exact-fit boundary
TEST(VirtualMemoryExtTest, LoadSegmentExactlyFillingSlotSucceeds)
{
    VmFix f;
    const std::size_t slotSize = 4;
    ASSERT_TRUE(f.vm.allocateSlot(1, slotSize).isOk());

    std::vector<Block> data(slotSize, Block{Instruction::Nop, 0, 0, 0});
    ASSERT_TRUE(f.vm.loadSegment(1, data).isOk());

    auto r = f.vm.readSegment(1);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value().size(), slotSize);
}

// Segment: reload overwrites previous content
TEST(VirtualMemoryExtTest, ReloadSegmentOverwritesPreviousContent)
{
    VmFix f;
    ASSERT_TRUE(f.vm.allocateSlot(1, 4).isOk());

    std::vector<Block> first = {
        {Instruction::Mov, 0, 1, 0},
        {Instruction::Halt, 0, 0, 0},
        {Instruction::Nop, 0, 0, 0},
        {Instruction::Nop, 0, 0, 0},
    };
    ASSERT_TRUE(f.vm.loadSegment(1, first).isOk());

    std::vector<Block> second = {
        {Instruction::Add, 0, 99, 0},
        {Instruction::Halt, 0, 0, 0},
        {Instruction::Nop, 0, 0, 0},
        {Instruction::Nop, 0, 0, 0},
    };
    ASSERT_TRUE(f.vm.loadSegment(1, second).isOk());

    auto r = f.vm.readSegment(1);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value()[0].code, Instruction::Add);
    EXPECT_EQ(r.value()[0].operand, 99);
}

// Segments from different processes are isolated
TEST(VirtualMemoryExtTest, SegmentsFromDifferentProcessesAreIsolated)
{
    VmFix f;
    ASSERT_TRUE(f.vm.allocateSlot(1, 3).isOk());
    ASSERT_TRUE(f.vm.allocateSlot(2, 3).isOk());

    std::vector<Block> p1code = {
        {Instruction::Mov, 0, 11, 0},
        {Instruction::Halt, 0, 0, 0},
        {Instruction::Nop, 0, 0, 0},
    };
    std::vector<Block> p2code = {
        {Instruction::Mov, 0, 22, 0},
        {Instruction::Halt, 0, 0, 0},
        {Instruction::Nop, 0, 0, 0},
    };
    ASSERT_TRUE(f.vm.loadSegment(1, p1code).isOk());
    ASSERT_TRUE(f.vm.loadSegment(2, p2code).isOk());

    auto r1 = f.vm.readSegment(1);
    auto r2 = f.vm.readSegment(2);
    ASSERT_TRUE(r1.isOk());
    ASSERT_TRUE(r2.isOk());
    EXPECT_EQ(r1.value()[0].operand, 11);
    EXPECT_EQ(r2.value()[0].operand, 22);
}

// hasSlot and slotSize consistency
TEST(VirtualMemoryExtTest, HasSlotAndSlotSizeAreConsistentAfterFree)
{
    VmFix f;
    ASSERT_TRUE(f.vm.allocateSlot(42, 5).isOk());
    EXPECT_TRUE(f.vm.hasSlot(42));
    EXPECT_EQ(f.vm.slotSize(42), 5u);

    ASSERT_TRUE(f.vm.freeSlot(42).isOk());
    EXPECT_FALSE(f.vm.hasSlot(42));
    EXPECT_EQ(f.vm.slotSize(42), 0u);
}
