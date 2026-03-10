/// @file isa.h
/// @brief ISA (Instruction Set Architecture) constants and helpers.
///
/// Provides constexpr utility functions for instruction decoding,
/// name lookup, and classification. All functions are header-only
/// and suitable for compile-time evaluation.

#pragma once

#include "contur/arch/instruction.h"
#include "contur/arch/interrupt.h"

#include <cstdint>
#include <string_view>

namespace contur {

    /// @brief Total number of defined instructions.
    constexpr std::uint8_t INSTRUCTION_COUNT = 25;

    /// @brief Returns the human-readable mnemonic for an instruction.
    [[nodiscard]] constexpr std::string_view instructionName(Instruction instr) noexcept
    {
        switch (instr) {
        case Instruction::Nop: return "NOP";
        case Instruction::Mov: return "MOV";
        case Instruction::Add: return "ADD";
        case Instruction::Sub: return "SUB";
        case Instruction::Mul: return "MUL";
        case Instruction::Div: return "DIV";
        case Instruction::And: return "AND";
        case Instruction::Or: return "OR";
        case Instruction::Xor: return "XOR";
        case Instruction::ShiftLeft: return "SHL";
        case Instruction::ShiftRight: return "SHR";
        case Instruction::Compare: return "CMP";
        case Instruction::JumpEqual: return "JE";
        case Instruction::JumpNotEqual: return "JNE";
        case Instruction::JumpGreater: return "JG";
        case Instruction::JumpLess: return "JL";
        case Instruction::JumpGreaterEqual: return "JGE";
        case Instruction::JumpLessEqual: return "JLE";
        case Instruction::Push: return "PUSH";
        case Instruction::Pop: return "POP";
        case Instruction::Call: return "CALL";
        case Instruction::Return: return "RET";
        case Instruction::ReadMemory: return "RMEM";
        case Instruction::WriteMemory: return "WMEM";
        case Instruction::Interrupt: return "INT";
        case Instruction::Halt: return "HALT";
        }
        return "???";
    }

    /// @brief Returns the human-readable name for an interrupt code.
    [[nodiscard]] constexpr std::string_view interruptName(Interrupt intr) noexcept
    {
        switch (intr) {
        case Interrupt::Ok: return "OK";
        case Interrupt::Error: return "ERROR";
        case Interrupt::DivByZero: return "DIV_BY_ZERO";
        case Interrupt::SystemCall: return "SYSCALL";
        case Interrupt::PageFault: return "PAGE_FAULT";
        case Interrupt::Exit: return "EXIT";
        case Interrupt::NetworkIO: return "NETWORK_IO";
        case Interrupt::Timer: return "TIMER";
        case Interrupt::DeviceIO: return "DEVICE_IO";
        }
        return "UNKNOWN";
    }

    /// @brief Returns true if the instruction is an arithmetic operation (Add, Sub, Mul, Div).
    [[nodiscard]] constexpr bool isArithmetic(Instruction instr) noexcept
    {
        return instr == Instruction::Add || instr == Instruction::Sub || instr == Instruction::Mul ||
            instr == Instruction::Div;
    }

    /// @brief Returns true if the instruction is a logic/bitwise operation (And, Or, Xor, Shift).
    [[nodiscard]] constexpr bool isLogic(Instruction instr) noexcept
    {
        return instr == Instruction::And || instr == Instruction::Or || instr == Instruction::Xor ||
            instr == Instruction::ShiftLeft || instr == Instruction::ShiftRight;
    }

    /// @brief Returns true if the instruction is a conditional or unconditional jump.
    [[nodiscard]] constexpr bool isJump(Instruction instr) noexcept
    {
        return instr == Instruction::JumpEqual || instr == Instruction::JumpNotEqual ||
            instr == Instruction::JumpGreater || instr == Instruction::JumpLess ||
            instr == Instruction::JumpGreaterEqual || instr == Instruction::JumpLessEqual;
    }

    /// @brief Returns true if the instruction affects the stack (Push, Pop, Call, Return).
    [[nodiscard]] constexpr bool isStackOp(Instruction instr) noexcept
    {
        return instr == Instruction::Push || instr == Instruction::Pop ||
            instr == Instruction::Call || instr == Instruction::Return;
    }

    /// @brief Returns true if the instruction involves memory access (ReadMemory, WriteMemory).
    [[nodiscard]] constexpr bool isMemoryOp(Instruction instr) noexcept
    {
        return instr == Instruction::ReadMemory || instr == Instruction::WriteMemory;
    }

    /// @brief Returns true if the instruction terminates execution flow (Halt, Interrupt with Exit).
    [[nodiscard]] constexpr bool isHalt(Instruction instr) noexcept
    {
        return instr == Instruction::Halt;
    }

} // namespace contur
