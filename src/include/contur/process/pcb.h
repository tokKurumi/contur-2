/// @file pcb.h
/// @brief Process Control Block — metadata for a single process.
///
/// The PCB stores all bookkeeping information about a process: its identity,
/// current state, priority, timing data, and virtual memory address info.

#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "contur/core/types.h"

#include "contur/process/priority.h"
#include "contur/process/state.h"

namespace contur {

    /// @brief Aggregate holding process timing statistics.
    struct ProcessTiming
    {
        Tick arrivalTime = 0;      ///< Tick when the process was created.
        Tick startTime = 0;        ///< Tick when the process first entered Running.
        Tick finishTime = 0;       ///< Tick when the process entered Terminated.
        Tick totalCpuTime = 0;     ///< Cumulative ticks spent in Running state.
        Tick totalWaitTime = 0;    ///< Cumulative ticks spent in Ready state.
        Tick totalBlockedTime = 0; ///< Cumulative ticks spent in Blocked state.
        Tick lastStateChange = 0;  ///< Tick of the most recent state transition.
        Tick estimatedBurst = 0;   ///< Predicted next CPU burst (for SPN/SRT).
        Tick remainingBurst = 0;   ///< Remaining ticks in current burst (for SRT).

        /// @brief Default constructor.
        constexpr ProcessTiming() noexcept = default;
    };

    /// @brief Aggregate holding virtual memory address mapping for a process.
    struct ProcessAddressInfo
    {
        MemoryAddress virtualBase = NULL_ADDRESS; ///< Base address in virtual memory.
        MemoryAddress codeStart = NULL_ADDRESS;   ///< Start of code segment.
        std::uint32_t codeSize = 0;               ///< Size of code segment (in blocks).
        std::uint32_t slot = 0;                   ///< Virtual memory slot index.

        /// @brief Default constructor.
        constexpr ProcessAddressInfo() noexcept = default;
    };

    /// @brief Process Control Block — stores all metadata for a single process.
    ///
    /// The PCB is a standalone class using composition. It contains process identity,
    /// state, priority, timing info, and address mapping — everything the scheduler,
    /// dispatcher, and MMU need to manage the process.
    class PCB
    {
      public:
        /// @brief Constructs a PCB with the given ID, name, and optional priority.
        /// @param id        Unique process identifier (must not be INVALID_PID).
        /// @param name      Human-readable process name.
        /// @param priority  Initial priority descriptor (defaults to Normal).
        /// @param arrivalTime Simulation tick when the process was created.
        PCB(ProcessId id, std::string name, Priority priority = Priority{}, Tick arrivalTime = 0);

        ~PCB();

        // Non-copyable
        PCB(const PCB &) = delete;
        PCB &operator=(const PCB &) = delete;

        // Movable
        PCB(PCB &&) noexcept;
        PCB &operator=(PCB &&) noexcept;

        // --- Identity ---

        /// @brief Returns the unique process ID.
        [[nodiscard]] ProcessId id() const noexcept;

        /// @brief Returns the human-readable process name.
        [[nodiscard]] std::string_view name() const noexcept;

        // --- State ---

        /// @brief Returns the current process state.
        [[nodiscard]] ProcessState state() const noexcept;

        /// @brief Transitions the process to a new state.
        /// @param newState The target state.
        /// @param currentTick The simulation tick at which the transition occurs.
        /// @return true if the transition was valid and applied, false otherwise.
        [[nodiscard]] bool setState(ProcessState newState, Tick currentTick = 0);

        // --- Priority ---

        /// @brief Returns a const reference to the process priority.
        [[nodiscard]] const Priority &priority() const noexcept;

        /// @brief Sets the full priority descriptor.
        void setPriority(const Priority &priority);

        /// @brief Sets only the effective priority level (for dynamic scheduling).
        void setEffectivePriority(PriorityLevel level);

        /// @brief Sets the nice value (clamped to valid range).
        void setNice(std::int32_t nice);

        // --- Timing ---

        /// @brief Returns a const reference to the process timing data.
        [[nodiscard]] const ProcessTiming &timing() const noexcept;

        /// @brief Returns a mutable reference to the timing data.
        [[nodiscard]] ProcessTiming &timing() noexcept;

        /// @brief Adds the given number of ticks to the total CPU time.
        void addCpuTime(Tick ticks);

        /// @brief Adds the given number of ticks to the total wait time.
        void addWaitTime(Tick ticks);

        /// @brief Adds the given number of ticks to the total blocked time.
        void addBlockedTime(Tick ticks);

        // --- Address Info ---

        /// @brief Returns a const reference to the address mapping info.
        [[nodiscard]] const ProcessAddressInfo &addressInfo() const noexcept;

        /// @brief Returns a mutable reference to the address mapping info.
        [[nodiscard]] ProcessAddressInfo &addressInfo() noexcept;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
