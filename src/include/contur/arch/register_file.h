/// @file register_file.h
/// @brief Register enum class and RegisterFile — the CPU's register bank.
///
/// The RegisterFile holds 16 general-purpose registers including the
/// Program Counter (PC) and Stack Pointer (SP). It provides safe
/// indexed access, reset, and debug output.

#pragma once

#include "contur/core/types.h"

#include <array>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>

namespace contur {

    /// @brief CPU register identifiers.
    ///
    /// 14 general-purpose registers (R0–R13) plus two special-purpose:
    /// - ProgramCounter (R14): points to the next instruction to execute
    /// - StackPointer (R15): points to the top of the process stack
    enum class Register : std::uint8_t {
        R0 = 0,
        R1,
        R2,
        R3,
        R4,
        R5,
        R6,
        R7,
        R8,
        R9,
        R10,
        R11,
        R12,
        R13,
        ProgramCounter = 14,
        StackPointer = 15,
    };

    /// @brief The CPU's register bank — holds REGISTER_COUNT (16) registers.
    ///
    /// Provides safe access by Register enum or raw index, bulk save/restore
    /// (for context switching), and formatted debug output.
    class RegisterFile
    {
    public:
        RegisterFile();
        ~RegisterFile();

        // Non-copyable
        RegisterFile(const RegisterFile&) = delete;
        RegisterFile& operator=(const RegisterFile&) = delete;

        // Movable
        RegisterFile(RegisterFile&&) noexcept;
        RegisterFile& operator=(RegisterFile&&) noexcept;

        /// @brief Gets the value of the specified register.
        [[nodiscard]] RegisterValue get(Register reg) const noexcept;

        /// @brief Gets the value of a register by raw index.
        /// @pre index < REGISTER_COUNT
        [[nodiscard]] RegisterValue get(std::uint8_t index) const noexcept;

        /// @brief Sets the value of the specified register.
        void set(Register reg, RegisterValue value) noexcept;

        /// @brief Sets the value of a register by raw index.
        /// @pre index < REGISTER_COUNT
        void set(std::uint8_t index, RegisterValue value) noexcept;

        /// @brief Convenience accessor for the Program Counter.
        [[nodiscard]] RegisterValue pc() const noexcept;

        /// @brief Sets the Program Counter.
        void setPc(RegisterValue value) noexcept;

        /// @brief Convenience accessor for the Stack Pointer.
        [[nodiscard]] RegisterValue sp() const noexcept;

        /// @brief Sets the Stack Pointer.
        void setSp(RegisterValue value) noexcept;

        /// @brief Resets all registers to zero.
        void reset() noexcept;

        /// @brief Returns a snapshot of all register values as an array.
        ///        Used for context switching (save/restore).
        [[nodiscard]] std::array<RegisterValue, REGISTER_COUNT> snapshot() const noexcept;

        /// @brief Restores register values from a previously taken snapshot.
        void restore(const std::array<RegisterValue, REGISTER_COUNT>& values) noexcept;

        /// @brief Returns a formatted string showing all register values.
        [[nodiscard]] std::string dump() const;

        /// @brief Stream output operator for debug printing.
        friend std::ostream& operator<<(std::ostream& os, const RegisterFile& rf);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

    /// @brief Returns the human-readable name of a register (e.g., "R0", "PC", "SP").
    [[nodiscard]] constexpr std::string_view registerName(Register reg) noexcept
    {
        switch (reg) {
        case Register::R0: return "R0";
        case Register::R1: return "R1";
        case Register::R2: return "R2";
        case Register::R3: return "R3";
        case Register::R4: return "R4";
        case Register::R5: return "R5";
        case Register::R6: return "R6";
        case Register::R7: return "R7";
        case Register::R8: return "R8";
        case Register::R9: return "R9";
        case Register::R10: return "R10";
        case Register::R11: return "R11";
        case Register::R12: return "R12";
        case Register::R13: return "R13";
        case Register::ProgramCounter: return "PC";
        case Register::StackPointer: return "SP";
        }
        return "??";
    }

} // namespace contur
