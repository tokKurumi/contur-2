/// @file test_pcb.cpp
/// @brief Unit tests for PCB (Process Control Block).

#include <gtest/gtest.h>

#include "contur/process/pcb.h"

using namespace contur;

// Construction

TEST(PCBTest, ConstructWithDefaults)
{
    PCB pcb(1, "init");
    EXPECT_EQ(pcb.id(), 1u);
    EXPECT_EQ(pcb.name(), "init");
    EXPECT_EQ(pcb.state(), ProcessState::New);
    EXPECT_EQ(pcb.priority().base, PriorityLevel::Normal);
    EXPECT_EQ(pcb.priority().effective, PriorityLevel::Normal);
    EXPECT_EQ(pcb.priority().nice, NICE_DEFAULT);
    EXPECT_EQ(pcb.timing().arrivalTime, 0u);
}

TEST(PCBTest, ConstructWithPriority)
{
    PCB pcb(2, "worker", Priority(PriorityLevel::High));
    EXPECT_EQ(pcb.id(), 2u);
    EXPECT_EQ(pcb.name(), "worker");
    EXPECT_EQ(pcb.priority().base, PriorityLevel::High);
    EXPECT_EQ(pcb.priority().effective, PriorityLevel::High);
}

TEST(PCBTest, ConstructWithArrivalTime)
{
    PCB pcb(3, "timer", Priority{}, 42);
    EXPECT_EQ(pcb.timing().arrivalTime, 42u);
    EXPECT_EQ(pcb.timing().lastStateChange, 42u);
}

// Move semantics

TEST(PCBTest, MoveConstruction)
{
    PCB original(1, "mover", Priority(PriorityLevel::High));
    PCB moved(std::move(original));
    EXPECT_EQ(moved.id(), 1u);
    EXPECT_EQ(moved.name(), "mover");
    EXPECT_EQ(moved.priority().base, PriorityLevel::High);
}

TEST(PCBTest, MoveAssignment)
{
    PCB a(1, "alpha");
    PCB b(2, "beta", Priority(PriorityLevel::Low));
    b = std::move(a);
    EXPECT_EQ(b.id(), 1u);
    EXPECT_EQ(b.name(), "alpha");
}

// State transitions

TEST(PCBTest, ValidTransitionNewToReady)
{
    PCB pcb(1, "proc");
    EXPECT_TRUE(pcb.setState(ProcessState::Ready, 10));
    EXPECT_EQ(pcb.state(), ProcessState::Ready);
}

TEST(PCBTest, InvalidTransitionNewToRunning)
{
    PCB pcb(1, "proc");
    EXPECT_FALSE(pcb.setState(ProcessState::Running, 5));
    EXPECT_EQ(pcb.state(), ProcessState::New);
}

TEST(PCBTest, FullLifecycleTransition)
{
    PCB pcb(1, "lifecycle", Priority{}, 0);
    EXPECT_TRUE(pcb.setState(ProcessState::Ready, 1));
    EXPECT_TRUE(pcb.setState(ProcessState::Running, 3));
    EXPECT_TRUE(pcb.setState(ProcessState::Blocked, 5));
    EXPECT_TRUE(pcb.setState(ProcessState::Ready, 8));
    EXPECT_TRUE(pcb.setState(ProcessState::Running, 10));
    EXPECT_TRUE(pcb.setState(ProcessState::Terminated, 15));
    EXPECT_EQ(pcb.state(), ProcessState::Terminated);
}

TEST(PCBTest, TerminatedIsTerminal)
{
    PCB pcb(1, "done");
    ASSERT_TRUE(pcb.setState(ProcessState::Ready, 1));
    ASSERT_TRUE(pcb.setState(ProcessState::Running, 2));
    ASSERT_TRUE(pcb.setState(ProcessState::Terminated, 3));
    EXPECT_FALSE(pcb.setState(ProcessState::Ready, 4));
    EXPECT_EQ(pcb.state(), ProcessState::Terminated);
}

// Timing tracking

TEST(PCBTest, TimingTracksStartTime)
{
    PCB pcb(1, "timed", Priority{}, 0);
    ASSERT_TRUE(pcb.setState(ProcessState::Ready, 1));
    ASSERT_TRUE(pcb.setState(ProcessState::Running, 5));
    EXPECT_EQ(pcb.timing().startTime, 5u);
}

TEST(PCBTest, TimingStartTimeSetOnce)
{
    PCB pcb(1, "timed", Priority{}, 0);
    ASSERT_TRUE(pcb.setState(ProcessState::Ready, 1));
    ASSERT_TRUE(pcb.setState(ProcessState::Running, 5));
    ASSERT_TRUE(pcb.setState(ProcessState::Ready, 8));    // preempted
    ASSERT_TRUE(pcb.setState(ProcessState::Running, 10)); // re-scheduled
    EXPECT_EQ(pcb.timing().startTime, 5u);                // Still first start
}

TEST(PCBTest, TimingTracksFinishTime)
{
    PCB pcb(1, "finisher", Priority{}, 0);
    ASSERT_TRUE(pcb.setState(ProcessState::Ready, 1));
    ASSERT_TRUE(pcb.setState(ProcessState::Running, 3));
    ASSERT_TRUE(pcb.setState(ProcessState::Terminated, 10));
    EXPECT_EQ(pcb.timing().finishTime, 10u);
}

TEST(PCBTest, TimingTracksCpuTime)
{
    PCB pcb(1, "cpu", Priority{}, 0);
    ASSERT_TRUE(pcb.setState(ProcessState::Ready, 0));
    ASSERT_TRUE(pcb.setState(ProcessState::Running, 2));
    ASSERT_TRUE(pcb.setState(ProcessState::Ready, 7)); // 5 ticks running
    EXPECT_EQ(pcb.timing().totalCpuTime, 5u);
}

TEST(PCBTest, TimingTracksWaitTime)
{
    PCB pcb(1, "waiter", Priority{}, 0);
    ASSERT_TRUE(pcb.setState(ProcessState::Ready, 0));
    ASSERT_TRUE(pcb.setState(ProcessState::Running, 4)); // 4 ticks waiting
    EXPECT_EQ(pcb.timing().totalWaitTime, 4u);
}

TEST(PCBTest, TimingTracksBlockedTime)
{
    PCB pcb(1, "blocked", Priority{}, 0);
    ASSERT_TRUE(pcb.setState(ProcessState::Ready, 0));
    ASSERT_TRUE(pcb.setState(ProcessState::Running, 1));
    ASSERT_TRUE(pcb.setState(ProcessState::Blocked, 3));
    ASSERT_TRUE(pcb.setState(ProcessState::Ready, 8)); // 5 ticks blocked
    EXPECT_EQ(pcb.timing().totalBlockedTime, 5u);
}

TEST(PCBTest, TimingCumulativeAcrossMultipleRuns)
{
    PCB pcb(1, "multi", Priority{}, 0);
    ASSERT_TRUE(pcb.setState(ProcessState::Ready, 0));
    ASSERT_TRUE(pcb.setState(ProcessState::Running, 2));     // 2 wait
    ASSERT_TRUE(pcb.setState(ProcessState::Blocked, 5));     // 3 cpu
    ASSERT_TRUE(pcb.setState(ProcessState::Ready, 9));       // 4 blocked
    ASSERT_TRUE(pcb.setState(ProcessState::Running, 11));    // 2 wait (total 4)
    ASSERT_TRUE(pcb.setState(ProcessState::Terminated, 14)); // 3 cpu (total 6)

    EXPECT_EQ(pcb.timing().totalCpuTime, 6u);
    EXPECT_EQ(pcb.timing().totalWaitTime, 4u);
    EXPECT_EQ(pcb.timing().totalBlockedTime, 4u);
    EXPECT_EQ(pcb.timing().finishTime, 14u);
}

TEST(PCBTest, ManualCpuTimeAddition)
{
    PCB pcb(1, "manual");
    pcb.addCpuTime(10);
    pcb.addCpuTime(5);
    EXPECT_EQ(pcb.timing().totalCpuTime, 15u);
}

TEST(PCBTest, ManualWaitTimeAddition)
{
    PCB pcb(1, "manual");
    pcb.addWaitTime(3);
    pcb.addWaitTime(7);
    EXPECT_EQ(pcb.timing().totalWaitTime, 10u);
}

TEST(PCBTest, ManualBlockedTimeAddition)
{
    PCB pcb(1, "manual");
    pcb.addBlockedTime(4);
    pcb.addBlockedTime(6);
    EXPECT_EQ(pcb.timing().totalBlockedTime, 10u);
}

// Priority manipulation

TEST(PCBTest, SetPriority)
{
    PCB pcb(1, "prio");
    Priority newPrio(PriorityLevel::High, PriorityLevel::Realtime, -10);
    pcb.setPriority(newPrio);
    EXPECT_EQ(pcb.priority().base, PriorityLevel::High);
    EXPECT_EQ(pcb.priority().effective, PriorityLevel::Realtime);
    EXPECT_EQ(pcb.priority().nice, -10);
}

TEST(PCBTest, SetEffectivePriority)
{
    PCB pcb(1, "dynamic", Priority(PriorityLevel::Normal));
    pcb.setEffectivePriority(PriorityLevel::High);
    EXPECT_EQ(pcb.priority().base, PriorityLevel::Normal);
    EXPECT_EQ(pcb.priority().effective, PriorityLevel::High);
}

TEST(PCBTest, SetNiceClamped)
{
    PCB pcb(1, "nice");
    pcb.setNice(5);
    EXPECT_EQ(pcb.priority().nice, 5);
    pcb.setNice(-100);
    EXPECT_EQ(pcb.priority().nice, NICE_MIN);
    pcb.setNice(100);
    EXPECT_EQ(pcb.priority().nice, NICE_MAX);
}

// Address info

TEST(PCBTest, AddressInfoDefaultValues)
{
    PCB pcb(1, "addr");
    EXPECT_EQ(pcb.addressInfo().virtualBase, NULL_ADDRESS);
    EXPECT_EQ(pcb.addressInfo().codeStart, NULL_ADDRESS);
    EXPECT_EQ(pcb.addressInfo().codeSize, 0u);
    EXPECT_EQ(pcb.addressInfo().slot, 0u);
}

TEST(PCBTest, AddressInfoMutable)
{
    PCB pcb(1, "addr");
    pcb.addressInfo().virtualBase = 0x1000;
    pcb.addressInfo().codeStart = 0x1000;
    pcb.addressInfo().codeSize = 256;
    pcb.addressInfo().slot = 3;

    EXPECT_EQ(pcb.addressInfo().virtualBase, 0x1000u);
    EXPECT_EQ(pcb.addressInfo().codeStart, 0x1000u);
    EXPECT_EQ(pcb.addressInfo().codeSize, 256u);
    EXPECT_EQ(pcb.addressInfo().slot, 3u);
}

TEST(PCBTest, TimingMutableReference)
{
    PCB pcb(1, "timing");
    pcb.timing().estimatedBurst = 10;
    pcb.timing().remainingBurst = 5;
    EXPECT_EQ(pcb.timing().estimatedBurst, 10u);
    EXPECT_EQ(pcb.timing().remainingBurst, 5u);
}
