/// @file types.h
/// @brief Common type aliases, sentinel constants, and forward declarations
///        used throughout the Contur 2 kernel simulator.

#pragma once

#include <cstdint>

namespace contur {

    /// @brief Unique identifier for a process.
    using ProcessId = std::uint32_t;

    /// @brief Represents a memory address (physical or virtual).
    using MemoryAddress = std::uint32_t;

    /// @brief Simulation clock tick counter.
    using Tick = std::uint64_t;

    /// @brief Value stored in a CPU register.
    using RegisterValue = std::int32_t;

    /// @brief Unique identifier for an I/O device.
    using DeviceId = std::uint16_t;

    /// @brief Unique identifier for a synchronization resource (mutex, semaphore, etc.).
    using ResourceId = std::uint32_t;

    /// @brief Frame number in physical memory.
    using FrameId = std::uint32_t;

    /// @brief Sentinel value indicating an invalid/unassigned process ID.
    constexpr ProcessId INVALID_PID = 0;

    /// @brief Sentinel value indicating an invalid/null memory address.
    constexpr MemoryAddress NULL_ADDRESS = 0xFFFFFFFF;

    /// @brief Sentinel value indicating an invalid frame.
    constexpr FrameId INVALID_FRAME = 0xFFFFFFFF;

    /// @brief Maximum number of CPU registers.
    constexpr std::uint8_t REGISTER_COUNT = 16;

    /// @brief Default time slice for Round Robin scheduling (in ticks).
    constexpr Tick DEFAULT_TIME_SLICE = 4;

    /// @brief Maximum number of processes the kernel can manage.
    constexpr ProcessId MAX_PROCESSES = 1024;

} // namespace contur
