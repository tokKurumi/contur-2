/// @file program_syscalls.h
/// @brief Inline syscall sequence emitters using the kernel register convention.
///
/// Each emitter appends one complete syscall sequence to a program buffer.
/// The calling convention (which registers carry the syscall id and arguments)
/// is defined in syscall_conventions.h.  Emitters handle the encoding so that
/// demo programs only express intent, not ABI details.

#pragma once

#include <cstdint>
#include <vector>

#include "contur/core/types.h"

#include "contur/arch/block.h"
#include "contur/arch/interrupt.h"
#include "contur/arch/program_builder.h"
#include "contur/fs/file_descriptor.h"
#include "contur/io/io_types.h"
#include "contur/syscall/syscall_conventions.h"
#include "contur/syscall/syscall_ids.h"

namespace contur {

    /// @brief Emits instructions to open a kernel I/O resource.
    ///
    /// Loads all arguments as immediates (resource id and flags are compile-time
    /// constants), then raises Interrupt::SystemCall.  On return the assigned
    /// I/O descriptor value is available in SYSCALL_ID_REGISTER (R0).
    ///
    /// @param code    Program buffer to append instructions to.
    /// @param resourceId Numeric resource identifier registered with IIoManager.
    /// @param kind    Resource kind (File, LanFile, SocketStream, etc.).
    /// @param mode    Open mode flags.
    inline void emitOpen(std::vector<Block> &code, RegisterValue resourceId, IoResourceKind kind, OpenMode mode)
    {
        constexpr auto kSyscall = SYSCALL_ID_REGISTER;
        constexpr auto kArg0 = SYSCALL_ARG_REGISTERS[0];
        constexpr auto kArg1 = SYSCALL_ARG_REGISTERS[1];
        constexpr auto kArg2 = SYSCALL_ARG_REGISTERS[2];
        constexpr auto kArg3 = SYSCALL_ARG_REGISTERS[3];

        code.push_back(movImm(kSyscall, static_cast<RegisterValue>(SyscallId::Open)));
        code.push_back(movImm(kArg0, resourceId));
        code.push_back(movImm(kArg1, static_cast<RegisterValue>(kind)));
        code.push_back(movImm(kArg2, static_cast<RegisterValue>(mode)));
        code.push_back(movImm(kArg3, 0));
        code.push_back(interrupt(Interrupt::SystemCall));
    }

    /// @brief Emits instructions to read one value from a kernel I/O descriptor.
    ///
    /// The descriptor must be held in @p fdReg at runtime.  On return the read
    /// value is available in SYSCALL_ID_REGISTER (R0).
    ///
    /// @param code  Program buffer to append instructions to.
    /// @param fdReg Register index holding the open descriptor value at runtime.
    inline void emitRead(std::vector<Block> &code, std::uint8_t fdReg)
    {
        constexpr auto kSyscall = SYSCALL_ID_REGISTER;
        constexpr auto kArg0 = SYSCALL_ARG_REGISTERS[0];

        code.push_back(movImm(kSyscall, static_cast<RegisterValue>(SyscallId::Read)));
        code.push_back(movReg(kArg0, fdReg));
        code.push_back(interrupt(Interrupt::SystemCall));
    }

    /// @brief Emits instructions to write one value to a kernel I/O descriptor.
    ///
    /// Both the descriptor and the payload must be held in registers at runtime.
    ///
    /// @param code   Program buffer to append instructions to.
    /// @param fdReg  Register index holding the open descriptor value at runtime.
    /// @param valReg Register index holding the value to write at runtime.
    inline void emitWrite(std::vector<Block> &code, std::uint8_t fdReg, std::uint8_t valReg)
    {
        constexpr auto kSyscall = SYSCALL_ID_REGISTER;
        constexpr auto kArg0 = SYSCALL_ARG_REGISTERS[0];
        constexpr auto kArg1 = SYSCALL_ARG_REGISTERS[1];

        code.push_back(movImm(kSyscall, static_cast<RegisterValue>(SyscallId::Write)));
        code.push_back(movReg(kArg0, fdReg));
        code.push_back(movReg(kArg1, valReg));
        code.push_back(interrupt(Interrupt::SystemCall));
    }

    /// @brief Emits instructions to close a kernel I/O descriptor.
    ///
    /// @param code  Program buffer to append instructions to.
    /// @param fdReg Register index holding the open descriptor value at runtime.
    inline void emitClose(std::vector<Block> &code, std::uint8_t fdReg)
    {
        constexpr auto kSyscall = SYSCALL_ID_REGISTER;
        constexpr auto kArg0 = SYSCALL_ARG_REGISTERS[0];

        code.push_back(movImm(kSyscall, static_cast<RegisterValue>(SyscallId::Close)));
        code.push_back(movReg(kArg0, fdReg));
        code.push_back(interrupt(Interrupt::SystemCall));
    }

} // namespace contur
