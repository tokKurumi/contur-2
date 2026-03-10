/// @file instruction.h
/// @brief Instruction enum class — all opcodes for the bytecode interpreter.

#pragma once

#include <cstdint>

namespace contur {

    /// @brief CPU instruction opcodes for the simulated architecture.
    ///
    /// Each instruction is a single byte, decoded by the CPU during the
    /// fetch-decode-execute cycle. The interpreter engine steps through
    /// these one at a time.
    enum class Instruction : std::uint8_t
    {
        Nop = 0,          ///< No operation
        Mov,              ///< Move immediate/register → register
        Add,              ///< Add: dst = dst + src
        Sub,              ///< Subtract: dst = dst - src
        Mul,              ///< Multiply: dst = dst * src
        Div,              ///< Divide: dst = dst / src (may raise DivByZero)
        And,              ///< Bitwise AND: dst = dst & src
        Or,               ///< Bitwise OR: dst = dst | src
        Xor,              ///< Bitwise XOR: dst = dst ^ src
        ShiftLeft,        ///< Left shift: dst = dst << src
        ShiftRight,       ///< Right shift: dst = dst >> src
        Compare,          ///< Compare two registers (sets flags)
        JumpEqual,        ///< Jump if equal (ZF set)
        JumpNotEqual,     ///< Jump if not equal (ZF clear)
        JumpGreater,      ///< Jump if greater
        JumpLess,         ///< Jump if less
        JumpGreaterEqual, ///< Jump if greater or equal
        JumpLessEqual,    ///< Jump if less or equal
        Push,             ///< Push register onto stack
        Pop,              ///< Pop top of stack into register
        Call,             ///< Call subroutine (push PC, jump)
        Return,           ///< Return from subroutine (pop PC)
        ReadMemory,       ///< Read value from memory address into register
        WriteMemory,      ///< Write register value to memory address
        Interrupt,        ///< Software interrupt (syscall, exit, etc.)
        Halt,             ///< Halt execution
    };

} // namespace contur
