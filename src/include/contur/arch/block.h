/// @file block.h
/// @brief Block — the fundamental unit of simulated memory.
///
/// Each Block represents a single cell in physical memory. In interpreted mode,
/// blocks store an instruction opcode, a register index, an integer operand,
/// and a state flag. This is the "Command" pattern — each block is a
/// self-contained instruction that the CPU can fetch, decode, and execute.

#pragma once

#include <cstdint>

#include "contur/arch/instruction.h"

namespace contur {

    /// @brief A single memory cell in the simulated architecture.
    ///
    /// Blocks serve dual purpose:
    /// - In the code segment: they represent instructions (opcode + operands)
    /// - In the data segment: the `operand` field stores data values
    ///
    /// This is a lightweight value type (Flyweight pattern) — designed to be
    /// stored in large arrays efficiently.
    struct Block
    {
        Instruction code = Instruction::Nop; ///< Instruction opcode
        std::uint8_t reg = 0;                ///< Target/source register index
        std::int32_t operand = 0;            ///< Immediate value or address
        std::int32_t state = 0;              ///< Auxiliary state / flags

        /// @brief Default-constructs a Nop block.
        constexpr Block() noexcept = default;

        /// @brief Constructs a block with all fields specified.
        constexpr Block(Instruction code, std::uint8_t reg, std::int32_t operand, std::int32_t state = 0) noexcept
            : code(code)
            , reg(reg)
            , operand(operand)
            , state(state)
        {}

        /// @brief Equality comparison.
        [[nodiscard]] constexpr bool operator==(const Block &other) const noexcept
        {
            return code == other.code && reg == other.reg && operand == other.operand && state == other.state;
        }

        /// @brief Inequality comparison.
        [[nodiscard]] constexpr bool operator!=(const Block &other) const noexcept
        {
            return !(*this == other);
        }
    };

} // namespace contur
