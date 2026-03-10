/// @file i_cpu.h
/// @brief ICPU — abstract interface for the CPU simulation.
///
/// The CPU performs the fetch-decode-execute cycle on a given register
/// context. The memory is accessed via IMemory, and results of each
/// cycle are communicated through Interrupt codes.

#pragma once

#include "contur/arch/interrupt.h"
#include "contur/arch/register_file.h"

namespace contur {

    /// @brief Abstract CPU interface.
    ///
    /// The CPU reads instructions from memory (referenced internally),
    /// decodes them, and executes using the provided RegisterFile.
    /// Each call to `step()` performs one fetch-decode-execute cycle.
    class ICPU
    {
        public:
        virtual ~ICPU() = default;

        /// @brief Performs a single fetch-decode-execute cycle.
        ///
        /// Reads the instruction at the address given by the Program Counter
        /// in @p regs, decodes it, executes it (updating @p regs and possibly
        /// memory), and advances the PC.
        ///
        /// @param regs The register file for the currently running process.
        /// @return An Interrupt code indicating the result:
        ///         - Interrupt::Ok         — normal execution, continue
        ///         - Interrupt::Exit       — process requested termination (Halt/Int Exit)
        ///         - Interrupt::DivByZero  — division by zero fault
        ///         - Interrupt::SystemCall — software interrupt (syscall)
        ///         - Interrupt::DeviceIO   — I/O interrupt
        ///         - Interrupt::Error      — generic error (invalid instruction, etc.)
        [[nodiscard]] virtual Interrupt step(RegisterFile &regs) = 0;

        /// @brief Resets the CPU's internal state (flags, etc.).
        virtual void reset() noexcept = 0;

        protected:
        ICPU() = default;
        ICPU(const ICPU &) = default;
        ICPU &operator=(const ICPU &) = default;
        ICPU(ICPU &&) = default;
        ICPU &operator=(ICPU &&) = default;
    };

} // namespace contur
