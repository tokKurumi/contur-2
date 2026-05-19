/// @file program_builder.h
/// @brief Inline Block construction helpers for assembling bytecode programs.
///
/// Each function produces one Block (the single memory-cell type the interpreter
/// fetches and executes).  state=0 → immediate operand; state=1 → register-indirect.

#pragma once

#include <cstdint>

#include "contur/core/types.h"

#include "contur/arch/block.h"
#include "contur/arch/instruction.h"
#include "contur/arch/interrupt.h"

namespace contur {

    /// @brief Loads an immediate value into a register.
    /// @param reg Destination register index.
    /// @param value Immediate operand.
    /// @return Block encoding the move-immediate instruction.
    [[nodiscard]] inline Block movImm(std::uint8_t reg, RegisterValue value) noexcept
    {
        return Block{Instruction::Mov, reg, value, 0};
    }

    /// @brief Copies the value of one register into another.
    /// @param dst Destination register index.
    /// @param src Source register index.
    /// @return Block encoding the move-register instruction.
    [[nodiscard]] inline Block movReg(std::uint8_t dst, std::uint8_t src) noexcept
    {
        return Block{Instruction::Mov, dst, static_cast<RegisterValue>(src), 1};
    }

    /// @brief Adds an immediate value to a register (dst = dst + value).
    /// @param reg Destination register index.
    /// @param value Immediate operand.
    /// @return Block encoding the add-immediate instruction.
    [[nodiscard]] inline Block addImm(std::uint8_t reg, RegisterValue value) noexcept
    {
        return Block{Instruction::Add, reg, value, 0};
    }

    /// @brief Adds the value of one register to another (dst = dst + src).
    /// @param dst Destination register index.
    /// @param src Source register index.
    /// @return Block encoding the add-register instruction.
    [[nodiscard]] inline Block addReg(std::uint8_t dst, std::uint8_t src) noexcept
    {
        return Block{Instruction::Add, dst, static_cast<RegisterValue>(src), 1};
    }

    /// @brief Subtracts an immediate value from a register (dst = dst - value).
    /// @param reg Destination register index.
    /// @param value Immediate operand.
    /// @return Block encoding the subtract-immediate instruction.
    [[nodiscard]] inline Block subImm(std::uint8_t reg, RegisterValue value) noexcept
    {
        return Block{Instruction::Sub, reg, value, 0};
    }

    /// @brief Subtracts the value of one register from another (dst = dst - src).
    /// @param dst Destination register index.
    /// @param src Source register index.
    /// @return Block encoding the subtract-register instruction.
    [[nodiscard]] inline Block subReg(std::uint8_t dst, std::uint8_t src) noexcept
    {
        return Block{Instruction::Sub, dst, static_cast<RegisterValue>(src), 1};
    }

    /// @brief Multiplies a register by an immediate value (dst = dst * value).
    /// @param reg Destination register index.
    /// @param value Immediate operand.
    /// @return Block encoding the multiply-immediate instruction.
    [[nodiscard]] inline Block mulImm(std::uint8_t reg, RegisterValue value) noexcept
    {
        return Block{Instruction::Mul, reg, value, 0};
    }

    /// @brief Multiplies a register by the value of another register (dst = dst * src).
    /// @param dst Destination register index.
    /// @param src Source register index.
    /// @return Block encoding the multiply-register instruction.
    [[nodiscard]] inline Block mulReg(std::uint8_t dst, std::uint8_t src) noexcept
    {
        return Block{Instruction::Mul, dst, static_cast<RegisterValue>(src), 1};
    }

    /// @brief Divides a register by an immediate value (dst = dst / value).
    /// @param reg Destination register index.
    /// @param value Immediate operand (must be non-zero at runtime).
    /// @return Block encoding the divide-immediate instruction.
    [[nodiscard]] inline Block divImm(std::uint8_t reg, RegisterValue value) noexcept
    {
        return Block{Instruction::Div, reg, value, 0};
    }

    /// @brief Divides a register by the value of another register (dst = dst / src).
    /// @param dst Destination register index.
    /// @param src Source register index (must be non-zero at runtime).
    /// @return Block encoding the divide-register instruction.
    [[nodiscard]] inline Block divReg(std::uint8_t dst, std::uint8_t src) noexcept
    {
        return Block{Instruction::Div, dst, static_cast<RegisterValue>(src), 1};
    }

    /// @brief Reads a value from a physical memory address into a register.
    /// @param reg Destination register index.
    /// @param addr Physical memory address.
    /// @return Block encoding the read-memory instruction.
    [[nodiscard]] inline Block readMem(std::uint8_t reg, MemoryAddress addr) noexcept
    {
        return Block{Instruction::ReadMemory, reg, static_cast<RegisterValue>(addr), 0};
    }

    /// @brief Writes a register value to a physical memory address.
    /// @param reg Source register index.
    /// @param addr Physical memory address.
    /// @return Block encoding the write-memory instruction.
    [[nodiscard]] inline Block writeMem(std::uint8_t reg, MemoryAddress addr) noexcept
    {
        return Block{Instruction::WriteMemory, reg, static_cast<RegisterValue>(addr), 0};
    }

    /// @brief Compares a register against an immediate value and sets flags.
    /// @param reg Register to compare.
    /// @param value Immediate operand.
    /// @return Block encoding the compare-immediate instruction.
    [[nodiscard]] inline Block compareImm(std::uint8_t reg, RegisterValue value) noexcept
    {
        return Block{Instruction::Compare, reg, value, 0};
    }

    /// @brief Compares two registers and sets flags.
    /// @param reg First register index.
    /// @param other Second register index.
    /// @return Block encoding the compare-register instruction.
    [[nodiscard]] inline Block compareReg(std::uint8_t reg, std::uint8_t other) noexcept
    {
        return Block{Instruction::Compare, reg, static_cast<RegisterValue>(other), 1};
    }

    /// @brief Jumps to @p target if the last compare was equal.
    /// @param target Instruction address to jump to.
    /// @return Block encoding the jump-equal instruction.
    [[nodiscard]] inline Block jumpEqual(RegisterValue target) noexcept
    {
        return Block{Instruction::JumpEqual, 0, target, 0};
    }

    /// @brief Jumps to @p target if the last compare was not equal.
    /// @param target Instruction address to jump to.
    /// @return Block encoding the jump-not-equal instruction.
    [[nodiscard]] inline Block jumpNotEqual(RegisterValue target) noexcept
    {
        return Block{Instruction::JumpNotEqual, 0, target, 0};
    }

    /// @brief Jumps to @p target if the last compare was greater.
    /// @param target Instruction address to jump to.
    /// @return Block encoding the jump-greater instruction.
    [[nodiscard]] inline Block jumpGreater(RegisterValue target) noexcept
    {
        return Block{Instruction::JumpGreater, 0, target, 0};
    }

    /// @brief Jumps to @p target if the last compare was less.
    /// @param target Instruction address to jump to.
    /// @return Block encoding the jump-less instruction.
    [[nodiscard]] inline Block jumpLess(RegisterValue target) noexcept
    {
        return Block{Instruction::JumpLess, 0, target, 0};
    }

    /// @brief Jumps to @p target if the last compare was greater or equal.
    /// @param target Instruction address to jump to.
    /// @return Block encoding the jump-greater-or-equal instruction.
    [[nodiscard]] inline Block jumpGreaterEqual(RegisterValue target) noexcept
    {
        return Block{Instruction::JumpGreaterEqual, 0, target, 0};
    }

    /// @brief Jumps to @p target if the last compare was less or equal.
    /// @param target Instruction address to jump to.
    /// @return Block encoding the jump-less-or-equal instruction.
    [[nodiscard]] inline Block jumpLessEqual(RegisterValue target) noexcept
    {
        return Block{Instruction::JumpLessEqual, 0, target, 0};
    }

    /// @brief Raises a software interrupt.
    /// @param code Interrupt kind (SystemCall, Exit, DeviceIO, etc.).
    /// @return Block encoding the interrupt instruction.
    [[nodiscard]] inline Block interrupt(Interrupt code) noexcept
    {
        return Block{Instruction::Interrupt, 0, static_cast<RegisterValue>(code), 0};
    }

    /// @brief No-op — advances the program counter by one.
    /// @return Block encoding the nop instruction.
    [[nodiscard]] inline Block nop() noexcept
    {
        return Block{Instruction::Nop, 0, 0, 0};
    }

    /// @brief Halts the process unconditionally.
    /// @return Block encoding the halt instruction.
    [[nodiscard]] inline Block halt() noexcept
    {
        return Block{Instruction::Halt, 0, 0, 0};
    }

} // namespace contur
