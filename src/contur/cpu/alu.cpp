/// @file alu.cpp
/// @brief ALU implementation — pure arithmetic and logic operations.

#include "contur/cpu/alu.h"

namespace contur {

    // Arithmetic
    Result<RegisterValue> ALU::add(RegisterValue a, RegisterValue b) const noexcept
    {
        return Result<RegisterValue>::ok(a + b);
    }

    Result<RegisterValue> ALU::sub(RegisterValue a, RegisterValue b) const noexcept
    {
        return Result<RegisterValue>::ok(a - b);
    }

    Result<RegisterValue> ALU::mul(RegisterValue a, RegisterValue b) const noexcept
    {
        return Result<RegisterValue>::ok(a * b);
    }

    Result<RegisterValue> ALU::div(RegisterValue a, RegisterValue b) const noexcept
    {
        if (b == 0)
        {
            return Result<RegisterValue>::error(ErrorCode::DivisionByZero);
        }
        return Result<RegisterValue>::ok(a / b);
    }

    // Logic / bitwise
    Result<RegisterValue> ALU::bitwiseAnd(RegisterValue a, RegisterValue b) const noexcept
    {
        return Result<RegisterValue>::ok(a & b);
    }

    Result<RegisterValue> ALU::bitwiseOr(RegisterValue a, RegisterValue b) const noexcept
    {
        return Result<RegisterValue>::ok(a | b);
    }

    Result<RegisterValue> ALU::bitwiseXor(RegisterValue a, RegisterValue b) const noexcept
    {
        return Result<RegisterValue>::ok(a ^ b);
    }

    Result<RegisterValue> ALU::shiftLeft(RegisterValue a, RegisterValue b) const noexcept
    {
        return Result<RegisterValue>::ok(a << b);
    }

    Result<RegisterValue> ALU::shiftRight(RegisterValue a, RegisterValue b) const noexcept
    {
        return Result<RegisterValue>::ok(a >> b);
    }

    // Comparison
    RegisterValue ALU::compare(RegisterValue a, RegisterValue b) const noexcept
    {
        RegisterValue flags = 0;
        if (a == b)
        {
            flags |= ZERO_FLAG;
        }
        if (a < b)
        {
            flags |= SIGN_FLAG;
        }
        return flags;
    }

} // namespace contur
