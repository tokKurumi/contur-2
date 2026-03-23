/// @file test_result.cpp
/// @brief Unit tests for Result<T> and Result<void>.

#include <string>

#include <gtest/gtest.h>

#include "contur/core/error.h"

using namespace contur;

TEST(ResultTest, OkHoldsValue)
{
    auto r = Result<int>::ok(42);
    EXPECT_TRUE(r.isOk());
    EXPECT_FALSE(r.isError());
    EXPECT_EQ(r.value(), 42);
}

TEST(ResultTest, ErrorHoldsCode)
{
    auto r = Result<int>::error(ErrorCode::OutOfMemory);
    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(r.isError());
    EXPECT_EQ(r.errorCode(), ErrorCode::OutOfMemory);
}

TEST(ResultTest, OkWithStringValue)
{
    auto r = Result<std::string>::ok("hello");
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), "hello");
}

TEST(ResultTest, MoveValue)
{
    auto r = Result<std::string>::ok("moveable");
    auto val = std::move(r).value();
    EXPECT_EQ(val, "moveable");
}

TEST(ResultTest, ValueOr_ReturnsValueWhenOk)
{
    auto r = Result<int>::ok(10);
    EXPECT_EQ(r.valueOr(99), 10);
}

TEST(ResultTest, ValueOr_ReturnsDefaultWhenError)
{
    auto r = Result<int>::error(ErrorCode::NotFound);
    EXPECT_EQ(r.valueOr(99), 99);
}

TEST(ResultTest, CopyResult)
{
    auto r1 = Result<int>::ok(7);
    auto r2 = r1; // copy
    EXPECT_TRUE(r2.isOk());
    EXPECT_EQ(r2.value(), 7);
}

TEST(ResultTest, MultipleErrorCodes)
{
    auto r1 = Result<int>::error(ErrorCode::DivisionByZero);
    auto r2 = Result<int>::error(ErrorCode::SegmentationFault);
    auto r3 = Result<int>::error(ErrorCode::InvalidInstruction);

    EXPECT_EQ(r1.errorCode(), ErrorCode::DivisionByZero);
    EXPECT_EQ(r2.errorCode(), ErrorCode::SegmentationFault);
    EXPECT_EQ(r3.errorCode(), ErrorCode::InvalidInstruction);
}

TEST(ResultVoidTest, OkIsSuccess)
{
    auto r = Result<void>::ok();
    EXPECT_TRUE(r.isOk());
    EXPECT_FALSE(r.isError());
    EXPECT_EQ(r.errorCode(), ErrorCode::Ok);
}

TEST(ResultVoidTest, ErrorHoldsCode)
{
    auto r = Result<void>::error(ErrorCode::PermissionDenied);
    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(r.isError());
    EXPECT_EQ(r.errorCode(), ErrorCode::PermissionDenied);
}

TEST(ErrorCodeTest, ToStringCoversAllCodes)
{
    EXPECT_EQ(errorCodeToString(ErrorCode::Ok), "Ok");
    EXPECT_EQ(errorCodeToString(ErrorCode::OutOfMemory), "OutOfMemory");
    EXPECT_EQ(errorCodeToString(ErrorCode::DivisionByZero), "DivisionByZero");
    EXPECT_EQ(errorCodeToString(ErrorCode::InvalidPid), "InvalidPid");
    EXPECT_EQ(errorCodeToString(ErrorCode::InvalidAddress), "InvalidAddress");
    EXPECT_EQ(errorCodeToString(ErrorCode::InvalidInstruction), "InvalidInstruction");
    EXPECT_EQ(errorCodeToString(ErrorCode::SegmentationFault), "SegmentationFault");
    EXPECT_EQ(errorCodeToString(ErrorCode::DeviceError), "DeviceError");
    EXPECT_EQ(errorCodeToString(ErrorCode::ProcessNotFound), "ProcessNotFound");
    EXPECT_EQ(errorCodeToString(ErrorCode::PermissionDenied), "PermissionDenied");
    EXPECT_EQ(errorCodeToString(ErrorCode::Timeout), "Timeout");
    EXPECT_EQ(errorCodeToString(ErrorCode::DeadlockDetected), "DeadlockDetected");
    EXPECT_EQ(errorCodeToString(ErrorCode::InvalidState), "InvalidState");
    EXPECT_EQ(errorCodeToString(ErrorCode::InvalidArgument), "InvalidArgument");
    EXPECT_EQ(errorCodeToString(ErrorCode::ResourceBusy), "ResourceBusy");
    EXPECT_EQ(errorCodeToString(ErrorCode::NotFound), "NotFound");
    EXPECT_EQ(errorCodeToString(ErrorCode::AlreadyExists), "AlreadyExists");
    EXPECT_EQ(errorCodeToString(ErrorCode::BufferFull), "BufferFull");
    EXPECT_EQ(errorCodeToString(ErrorCode::BufferEmpty), "BufferEmpty");
    EXPECT_EQ(errorCodeToString(ErrorCode::EndOfFile), "EndOfFile");
    EXPECT_EQ(errorCodeToString(ErrorCode::NotImplemented), "NotImplemented");
}
