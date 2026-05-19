/// @file syscall_conventions.h
/// @brief Register conventions for system call dispatch.

#pragma once

#include <array>
#include <cstdint>

#include "contur/arch/register_file.h"

namespace contur {

    /// @brief Register used to pass syscall id and receive return value.
    constexpr std::uint8_t SYSCALL_ID_REGISTER = static_cast<std::uint8_t>(Register::R0);

    /// @brief Registers used to pass syscall arguments.
    constexpr std::array<std::uint8_t, 4> SYSCALL_ARG_REGISTERS = {
        static_cast<std::uint8_t>(Register::R1),
        static_cast<std::uint8_t>(Register::R2),
        static_cast<std::uint8_t>(Register::R3),
        static_cast<std::uint8_t>(Register::R4),
    };

    /// @brief Register used to store syscall status/error code.
    constexpr std::uint8_t SYSCALL_STATUS_REGISTER = static_cast<std::uint8_t>(Register::R7);

} // namespace contur
