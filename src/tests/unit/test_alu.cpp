/// @file test_alu.cpp
/// @brief Unit tests for ALU — Arithmetic Logic Unit.

#include <gtest/gtest.h>

#include "contur/cpu/alu.h"

using namespace contur;

class ALUTest : public ::testing::Test
{
  protected:
    ALU alu;
};

// Addition

TEST_F(ALUTest, AddPositive)
{
    auto r = alu.add(10, 20);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 30);
}

TEST_F(ALUTest, AddNegative)
{
    auto r = alu.add(-5, -3);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), -8);
}

TEST_F(ALUTest, AddMixed)
{
    auto r = alu.add(10, -3);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 7);
}

TEST_F(ALUTest, AddZero)
{
    auto r = alu.add(42, 0);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 42);
}

// Subtraction

TEST_F(ALUTest, SubPositive)
{
    auto r = alu.sub(20, 10);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 10);
}

TEST_F(ALUTest, SubNegativeResult)
{
    auto r = alu.sub(5, 10);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), -5);
}

TEST_F(ALUTest, SubZero)
{
    auto r = alu.sub(42, 0);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 42);
}

// Multiplication

TEST_F(ALUTest, MulPositive)
{
    auto r = alu.mul(6, 7);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 42);
}

TEST_F(ALUTest, MulByZero)
{
    auto r = alu.mul(100, 0);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 0);
}

TEST_F(ALUTest, MulNegative)
{
    auto r = alu.mul(-3, 4);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), -12);
}

TEST_F(ALUTest, MulBothNegative)
{
    auto r = alu.mul(-3, -4);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 12);
}

// Division

TEST_F(ALUTest, DivPositive)
{
    auto r = alu.div(42, 6);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 7);
}

TEST_F(ALUTest, DivIntegerTruncation)
{
    auto r = alu.div(10, 3);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 3);
}

TEST_F(ALUTest, DivByZero)
{
    auto r = alu.div(42, 0);
    ASSERT_TRUE(r.isError());
    EXPECT_EQ(r.errorCode(), ErrorCode::DivisionByZero);
}

TEST_F(ALUTest, DivNegative)
{
    auto r = alu.div(-12, 4);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), -3);
}

TEST_F(ALUTest, DivZeroByNonZero)
{
    auto r = alu.div(0, 5);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 0);
}

// Bitwise AND

TEST_F(ALUTest, BitwiseAndBasic)
{
    auto r = alu.bitwiseAnd(0b1100, 0b1010);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 0b1000);
}

TEST_F(ALUTest, BitwiseAndWithZero)
{
    auto r = alu.bitwiseAnd(0xFF, 0);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 0);
}

TEST_F(ALUTest, BitwiseAndWithAllOnes)
{
    auto r = alu.bitwiseAnd(0b1010, 0b1111);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 0b1010);
}

// Bitwise OR

TEST_F(ALUTest, BitwiseOrBasic)
{
    auto r = alu.bitwiseOr(0b1100, 0b0011);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 0b1111);
}

TEST_F(ALUTest, BitwiseOrWithZero)
{
    auto r = alu.bitwiseOr(0b1010, 0);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 0b1010);
}

// Bitwise XOR

TEST_F(ALUTest, BitwiseXorBasic)
{
    auto r = alu.bitwiseXor(0b1100, 0b1010);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 0b0110);
}

TEST_F(ALUTest, BitwiseXorSameValue)
{
    auto r = alu.bitwiseXor(42, 42);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 0);
}

// Shift left

TEST_F(ALUTest, ShiftLeftBasic)
{
    auto r = alu.shiftLeft(1, 3);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 8);
}

TEST_F(ALUTest, ShiftLeftByZero)
{
    auto r = alu.shiftLeft(42, 0);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 42);
}

// Shift right

TEST_F(ALUTest, ShiftRightBasic)
{
    auto r = alu.shiftRight(16, 2);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 4);
}

TEST_F(ALUTest, ShiftRightByZero)
{
    auto r = alu.shiftRight(42, 0);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 42);
}

// Compare

TEST_F(ALUTest, CompareEqual)
{
    RegisterValue flags = alu.compare(5, 5);
    EXPECT_NE(flags & ALU::ZERO_FLAG, 0);
    EXPECT_EQ(flags & ALU::SIGN_FLAG, 0);
}

TEST_F(ALUTest, CompareLessThan)
{
    RegisterValue flags = alu.compare(3, 7);
    EXPECT_EQ(flags & ALU::ZERO_FLAG, 0);
    EXPECT_NE(flags & ALU::SIGN_FLAG, 0);
}

TEST_F(ALUTest, CompareGreaterThan)
{
    RegisterValue flags = alu.compare(10, 3);
    EXPECT_EQ(flags & ALU::ZERO_FLAG, 0);
    EXPECT_EQ(flags & ALU::SIGN_FLAG, 0);
}

TEST_F(ALUTest, CompareNegativeValues)
{
    RegisterValue flags = alu.compare(-5, -3);
    EXPECT_EQ(flags & ALU::ZERO_FLAG, 0);
    EXPECT_NE(flags & ALU::SIGN_FLAG, 0); // -5 < -3
}

TEST_F(ALUTest, CompareZeros)
{
    RegisterValue flags = alu.compare(0, 0);
    EXPECT_NE(flags & ALU::ZERO_FLAG, 0);
    EXPECT_EQ(flags & ALU::SIGN_FLAG, 0);
}
