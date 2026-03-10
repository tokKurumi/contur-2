/// @file test_process_state.cpp
/// @brief Unit tests for ProcessState enum and isValidTransition().

#include <gtest/gtest.h>

#include "contur/process/state.h"

using namespace contur;

// processStateName

TEST(ProcessStateTest, NameReturnsCorrectStrings)
{
    EXPECT_EQ(processStateName(ProcessState::New), "New");
    EXPECT_EQ(processStateName(ProcessState::Ready), "Ready");
    EXPECT_EQ(processStateName(ProcessState::Running), "Running");
    EXPECT_EQ(processStateName(ProcessState::Blocked), "Blocked");
    EXPECT_EQ(processStateName(ProcessState::Suspended), "Suspended");
    EXPECT_EQ(processStateName(ProcessState::Terminated), "Terminated");
}

// Valid transitions

TEST(ProcessStateTest, NewToReady)
{
    EXPECT_TRUE(isValidTransition(ProcessState::New, ProcessState::Ready));
}

TEST(ProcessStateTest, ReadyToRunning)
{
    EXPECT_TRUE(isValidTransition(ProcessState::Ready, ProcessState::Running));
}

TEST(ProcessStateTest, ReadyToSuspended)
{
    EXPECT_TRUE(isValidTransition(ProcessState::Ready, ProcessState::Suspended));
}

TEST(ProcessStateTest, RunningToReady)
{
    EXPECT_TRUE(isValidTransition(ProcessState::Running, ProcessState::Ready));
}

TEST(ProcessStateTest, RunningToBlocked)
{
    EXPECT_TRUE(isValidTransition(ProcessState::Running, ProcessState::Blocked));
}

TEST(ProcessStateTest, RunningToTerminated)
{
    EXPECT_TRUE(isValidTransition(ProcessState::Running, ProcessState::Terminated));
}

TEST(ProcessStateTest, BlockedToReady)
{
    EXPECT_TRUE(isValidTransition(ProcessState::Blocked, ProcessState::Ready));
}

TEST(ProcessStateTest, BlockedToSuspended)
{
    EXPECT_TRUE(isValidTransition(ProcessState::Blocked, ProcessState::Suspended));
}

TEST(ProcessStateTest, SuspendedToReady)
{
    EXPECT_TRUE(isValidTransition(ProcessState::Suspended, ProcessState::Ready));
}

// Invalid transitions

TEST(ProcessStateTest, NewToRunningIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::New, ProcessState::Running));
}

TEST(ProcessStateTest, NewToBlockedIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::New, ProcessState::Blocked));
}

TEST(ProcessStateTest, NewToTerminatedIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::New, ProcessState::Terminated));
}

TEST(ProcessStateTest, NewToSuspendedIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::New, ProcessState::Suspended));
}

TEST(ProcessStateTest, NewToNewIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::New, ProcessState::New));
}

TEST(ProcessStateTest, ReadyToBlockedIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::Ready, ProcessState::Blocked));
}

TEST(ProcessStateTest, ReadyToTerminatedIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::Ready, ProcessState::Terminated));
}

TEST(ProcessStateTest, ReadyToNewIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::Ready, ProcessState::New));
}

TEST(ProcessStateTest, RunningToNewIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::Running, ProcessState::New));
}

TEST(ProcessStateTest, RunningToSuspendedIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::Running, ProcessState::Suspended));
}

TEST(ProcessStateTest, BlockedToRunningIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::Blocked, ProcessState::Running));
}

TEST(ProcessStateTest, BlockedToTerminatedIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::Blocked, ProcessState::Terminated));
}

TEST(ProcessStateTest, BlockedToNewIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::Blocked, ProcessState::New));
}

TEST(ProcessStateTest, SuspendedToRunningIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::Suspended, ProcessState::Running));
}

TEST(ProcessStateTest, SuspendedToBlockedIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::Suspended, ProcessState::Blocked));
}

TEST(ProcessStateTest, SuspendedToTerminatedIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::Suspended, ProcessState::Terminated));
}

TEST(ProcessStateTest, SuspendedToNewIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::Suspended, ProcessState::New));
}

TEST(ProcessStateTest, TerminatedToAnythingIsInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::Terminated, ProcessState::New));
    EXPECT_FALSE(isValidTransition(ProcessState::Terminated, ProcessState::Ready));
    EXPECT_FALSE(isValidTransition(ProcessState::Terminated, ProcessState::Running));
    EXPECT_FALSE(isValidTransition(ProcessState::Terminated, ProcessState::Blocked));
    EXPECT_FALSE(isValidTransition(ProcessState::Terminated, ProcessState::Suspended));
    EXPECT_FALSE(isValidTransition(ProcessState::Terminated, ProcessState::Terminated));
}

// Self-transitions are invalid for all states

TEST(ProcessStateTest, SelfTransitionsAreInvalid)
{
    EXPECT_FALSE(isValidTransition(ProcessState::New, ProcessState::New));
    EXPECT_FALSE(isValidTransition(ProcessState::Ready, ProcessState::Ready));
    EXPECT_FALSE(isValidTransition(ProcessState::Running, ProcessState::Running));
    EXPECT_FALSE(isValidTransition(ProcessState::Blocked, ProcessState::Blocked));
    EXPECT_FALSE(isValidTransition(ProcessState::Suspended, ProcessState::Suspended));
    EXPECT_FALSE(isValidTransition(ProcessState::Terminated, ProcessState::Terminated));
}

// Constexpr validation

TEST(ProcessStateTest, IsValidTransitionIsConstexpr)
{
    // Verify the function can be evaluated at compile time
    static_assert(isValidTransition(ProcessState::New, ProcessState::Ready));
    static_assert(!isValidTransition(ProcessState::Terminated, ProcessState::Ready));
    static_assert(isValidTransition(ProcessState::Running, ProcessState::Terminated));
    static_assert(!isValidTransition(ProcessState::Ready, ProcessState::Blocked));
}
