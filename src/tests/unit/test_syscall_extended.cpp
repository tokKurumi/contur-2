/// @file test_syscall_extended.cpp
/// @brief Extended unit tests for SyscallTable — multiple handlers, error
///        propagation, arg forwarding, and handler lifecycle.

#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "contur/arch/instruction.h"
#include "contur/core/error.h"
#include "contur/process/process_image.h"
#include "contur/syscall/syscall_ids.h"
#include "contur/syscall/syscall_table.h"

using namespace contur;

namespace {

    ProcessImage makeCaller(ProcessId pid = 1)
    {
        return ProcessImage(pid, "test-caller", {{Instruction::Nop, 0, 0, 0}});
    }

} // namespace

// ---------------------------------------------------------------------------
// Error propagation
// ---------------------------------------------------------------------------

TEST(SyscallExtTest, DispatchPropagatesHandlerError)
{
    SyscallTable table;
    auto caller = makeCaller();
    ASSERT_TRUE(
        table
            .registerHandler(
                SyscallId::Read,
                [](std::span<const RegisterValue>, ProcessImage &) {
                    return Result<RegisterValue>::error(ErrorCode::PermissionDenied);
                }
            )
            .isOk()
    );

    auto result = table.dispatch(SyscallId::Read, {}, caller);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::PermissionDenied);
}

TEST(SyscallExtTest, DispatchUnregisteredIdReturnsNotFound)
{
    SyscallTable table;
    auto caller = makeCaller();
    EXPECT_FALSE(table.hasHandler(SyscallId::Fork));

    auto result = table.dispatch(SyscallId::Fork, {}, caller);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

// ---------------------------------------------------------------------------
// Multiple handlers
// ---------------------------------------------------------------------------

TEST(SyscallExtTest, RegisterMultipleHandlersAllDispatchable)
{
    SyscallTable table;
    auto caller = makeCaller();

    const std::vector<std::pair<SyscallId, RegisterValue>> entries = {
        {SyscallId::Exit, 0},
        {SyscallId::Write, 1},
        {SyscallId::Read, 2},
        {SyscallId::GetPid, 3},
        {SyscallId::GetTime, 4},
        {SyscallId::Yield, 5},
    };

    for (auto [id, retVal] : entries)
    {
        ASSERT_TRUE(
            table
                .registerHandler(
                    id,
                    [retVal](std::span<const RegisterValue>, ProcessImage &) {
                        return Result<RegisterValue>::ok(retVal);
                    }
                )
                .isOk()
        );
    }

    EXPECT_EQ(table.handlerCount(), entries.size());

    for (auto [id, retVal] : entries)
    {
        ASSERT_TRUE(table.hasHandler(id)) << "missing handler for id " << static_cast<int>(id);
        auto result = table.dispatch(id, {}, caller);
        ASSERT_TRUE(result.isOk());
        EXPECT_EQ(result.value(), retVal);
    }
}

TEST(SyscallExtTest, HandlerCountDecreasesAfterUnregister)
{
    SyscallTable table;
    ASSERT_TRUE(
        table
            .registerHandler(
                SyscallId::Exit, [](std::span<const RegisterValue>, ProcessImage &) {
                    return Result<RegisterValue>::ok(0);
                }
            )
            .isOk()
    );
    ASSERT_TRUE(
        table
            .registerHandler(
                SyscallId::Yield, [](std::span<const RegisterValue>, ProcessImage &) {
                    return Result<RegisterValue>::ok(0);
                }
            )
            .isOk()
    );

    EXPECT_EQ(table.handlerCount(), 2u);
    ASSERT_TRUE(table.unregisterHandler(SyscallId::Exit).isOk());
    EXPECT_EQ(table.handlerCount(), 1u);
    EXPECT_FALSE(table.hasHandler(SyscallId::Exit));
    EXPECT_TRUE(table.hasHandler(SyscallId::Yield));
}

TEST(SyscallExtTest, HasHandlerReturnsFalseBeforeRegistration)
{
    SyscallTable table;
    EXPECT_FALSE(table.hasHandler(SyscallId::GetPid));
    EXPECT_EQ(table.handlerCount(), 0u);
}

// ---------------------------------------------------------------------------
// Argument forwarding
// ---------------------------------------------------------------------------

TEST(SyscallExtTest, HandlerReceivesAllArgsAndCallerPid)
{
    SyscallTable table;
    auto caller = makeCaller(99);

    std::vector<RegisterValue> capturedArgs;
    ProcessId capturedPid = INVALID_PID;

    ASSERT_TRUE(
        table
            .registerHandler(
                SyscallId::Write,
                [&capturedArgs, &capturedPid](std::span<const RegisterValue> args, ProcessImage &process) {
                    capturedArgs = std::vector<RegisterValue>(args.begin(), args.end());
                    capturedPid = process.id();
                    return Result<RegisterValue>::ok(static_cast<RegisterValue>(args.size()));
                }
            )
            .isOk()
    );

    const std::vector<RegisterValue> testArgs = {10, 20, 30, 40};
    auto result = table.dispatch(SyscallId::Write, testArgs, caller);

    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value(), static_cast<RegisterValue>(testArgs.size()));
    EXPECT_EQ(capturedArgs, testArgs);
    EXPECT_EQ(capturedPid, 99u);
}

TEST(SyscallExtTest, EmptyArgsForwardedCorrectly)
{
    SyscallTable table;
    auto caller = makeCaller();
    std::size_t argCount = 999;

    ASSERT_TRUE(
        table
            .registerHandler(
                SyscallId::Yield,
                [&argCount](std::span<const RegisterValue> args, ProcessImage &) {
                    argCount = args.size();
                    return Result<RegisterValue>::ok(0);
                }
            )
            .isOk()
    );

    auto result = table.dispatch(SyscallId::Yield, {}, caller);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(argCount, 0u);
}

// ---------------------------------------------------------------------------
// Common syscall ids usable end-to-end
// ---------------------------------------------------------------------------

TEST(SyscallExtTest, GetPidHandlerReturnsCaller)
{
    SyscallTable table;
    auto caller = makeCaller(42);

    ASSERT_TRUE(
        table
            .registerHandler(
                SyscallId::GetPid,
                [](std::span<const RegisterValue>, ProcessImage &p) {
                    return Result<RegisterValue>::ok(static_cast<RegisterValue>(p.id()));
                }
            )
            .isOk()
    );

    auto result = table.dispatch(SyscallId::GetPid, {}, caller);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value(), 42);
}

TEST(SyscallExtTest, GetTimeHandlerReturnsSimulatedTime)
{
    SyscallTable table;
    auto caller = makeCaller();
    constexpr RegisterValue simTime = 777;

    ASSERT_TRUE(
        table
            .registerHandler(
                SyscallId::GetTime,
                [](std::span<const RegisterValue>, ProcessImage &) {
                    return Result<RegisterValue>::ok(static_cast<RegisterValue>(777));
                }
            )
            .isOk()
    );

    auto result = table.dispatch(SyscallId::GetTime, {}, caller);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value(), simTime);
}
