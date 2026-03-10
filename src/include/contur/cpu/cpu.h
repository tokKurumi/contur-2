/// @file cpu.h
/// @brief Cpu — concrete implementation of the ICPU interface.
///
/// The Cpu owns an ALU and references an IMemory for instruction fetching
/// and memory read/write operations. It implements the full
/// fetch-decode-execute cycle with support for all ISA instructions.

#pragma once

#include <memory>

#include "contur/cpu/i_cpu.h"
#include "contur/memory/i_memory.h"

namespace contur {

    /// @brief Concrete CPU implementation.
    ///
    /// The CPU performs the fetch-decode-execute cycle:
    /// 1. **Fetch**: read Block from memory at address pointed to by PC
    /// 2. **Decode**: interpret the Block's fields (opcode, register, operand, state)
    /// 3. **Execute**: perform the operation (ALU for arithmetic/logic, direct for control flow)
    ///
    /// Instruction encoding in Block:
    /// - `code`   — the Instruction opcode
    /// - `reg`    — target (or source) register index
    /// - `operand` — immediate value, memory address, or second register index
    /// - `state`  — addressing mode: 0 = immediate, 1 = register-register
    class Cpu final : public ICPU
    {
        public:
        /// @brief Constructs a CPU connected to the given memory.
        /// @param memory Reference to the memory subsystem (must outlive the CPU).
        explicit Cpu(IMemory &memory);

        ~Cpu() override;

        // Non-copyable, movable
        Cpu(const Cpu &) = delete;
        Cpu &operator=(const Cpu &) = delete;
        Cpu(Cpu &&) noexcept;
        Cpu &operator=(Cpu &&) noexcept;

        /// @copydoc ICPU::step
        [[nodiscard]] Interrupt step(RegisterFile &regs) override;

        /// @copydoc ICPU::reset
        void reset() noexcept override;

        /// @brief Returns the current comparison flags.
        ///
        /// Flags are set by the Compare instruction:
        /// - Bit 0 (ZERO_FLAG): set if operands were equal
        /// - Bit 1 (SIGN_FLAG): set if first operand was less than second
        [[nodiscard]] RegisterValue flags() const noexcept;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
