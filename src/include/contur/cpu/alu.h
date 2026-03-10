/// @file alu.h
/// @brief ALU — Arithmetic Logic Unit for the simulated CPU.
///
/// The ALU performs pure arithmetic and logic operations on register values.
/// It is stateless — all inputs and outputs are passed explicitly.
/// Division by zero is detected and reported via Result<RegisterValue>.

#pragma once

#include "contur/core/error.h"
#include "contur/core/types.h"

namespace contur {

    /// @brief Arithmetic Logic Unit — performs all computational operations.
    ///
    /// The ALU is owned by the CPU and invoked during the execute phase
    /// of the fetch-decode-execute cycle. All methods are pure functions:
    /// they take operands and return results without side effects.
    class ALU
    {
        public:
        ALU() = default;
        ~ALU() = default;

        // Copyable, movable (no state)
        ALU(const ALU &) = default;
        ALU &operator=(const ALU &) = default;
        ALU(ALU &&) noexcept = default;
        ALU &operator=(ALU &&) noexcept = default;

        // Arithmetic operations

        /// @brief Addition: a + b.
        [[nodiscard]] Result<RegisterValue> add(RegisterValue a, RegisterValue b) const noexcept;

        /// @brief Subtraction: a - b.
        [[nodiscard]] Result<RegisterValue> sub(RegisterValue a, RegisterValue b) const noexcept;

        /// @brief Multiplication: a * b.
        [[nodiscard]] Result<RegisterValue> mul(RegisterValue a, RegisterValue b) const noexcept;

        /// @brief Division: a / b.
        /// @return DivisionByZero error if b == 0.
        [[nodiscard]] Result<RegisterValue> div(RegisterValue a, RegisterValue b) const noexcept;

        // Logic / bitwise operations

        /// @brief Bitwise AND: a & b.
        [[nodiscard]] Result<RegisterValue> bitwiseAnd(RegisterValue a, RegisterValue b) const noexcept;

        /// @brief Bitwise OR: a | b.
        [[nodiscard]] Result<RegisterValue> bitwiseOr(RegisterValue a, RegisterValue b) const noexcept;

        /// @brief Bitwise XOR: a ^ b.
        [[nodiscard]] Result<RegisterValue> bitwiseXor(RegisterValue a, RegisterValue b) const noexcept;

        /// @brief Left shift: a << b.
        [[nodiscard]] Result<RegisterValue> shiftLeft(RegisterValue a, RegisterValue b) const noexcept;

        /// @brief Right shift: a >> b (arithmetic shift).
        [[nodiscard]] Result<RegisterValue> shiftRight(RegisterValue a, RegisterValue b) const noexcept;

        // Comparison

        /// @brief Compares two values and returns a flags word.
        ///
        /// The returned value encodes comparison flags:
        /// - Bit 0 (ZERO_FLAG):  set if a == b
        /// - Bit 1 (SIGN_FLAG):  set if a < b (signed comparison)
        ///
        /// @return A flags word (never fails).
        [[nodiscard]] RegisterValue compare(RegisterValue a, RegisterValue b) const noexcept;

        // Flag constants

        static constexpr RegisterValue ZERO_FLAG = 1; ///< a == b
        static constexpr RegisterValue SIGN_FLAG = 2; ///< a < b
    };

} // namespace contur
