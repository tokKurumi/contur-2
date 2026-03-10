/// @file interrupt.h
/// @brief Interrupt enum class — hardware and software interrupt codes.

#pragma once

#include <cstdint>

namespace contur {

    /// @brief Interrupt codes used by the CPU and kernel.
    ///
    /// Positive values represent hardware/timer interrupts.
    /// Negative values indicate error conditions.
    /// The CPU generates these during execution; the dispatcher
    /// and kernel handle them to drive process state transitions.
    enum class Interrupt : std::int32_t
    {
        Ok = 0,          ///< No interrupt — normal execution
        Error = -1,      ///< Generic error
        DivByZero = 3,   ///< Division by zero fault
        SystemCall = 11, ///< Software interrupt — syscall entry
        PageFault = 14,  ///< Page not present in physical memory
        Exit = 16,       ///< Process requests termination
        NetworkIO = 32,  ///< Network I/O event
        Timer = 64,      ///< Timer tick interrupt (preemption)
        DeviceIO = 254,  ///< Generic device I/O interrupt
    };

} // namespace contur
