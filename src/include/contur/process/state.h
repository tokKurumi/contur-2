/// @file state.h
/// @brief ProcessState enum class and validated state transitions.
///
/// Defines all possible states a process can be in during its lifecycle and
/// provides a constexpr validation function for transition legality.
///
/// State diagram:
/// @code
///   New ──→ Ready ──→ Running ──→ Terminated
///             ↑           │  ↑
///             │           ▼  │
///             └─────── Blocked
///             ↑           │
///             │           ▼
///          Suspended ←────┘
///             │
///             └──→ Ready
/// @endcode

#pragma once

#include <cstdint>
#include <string_view>

namespace contur {

    /// @brief All possible states in a process lifecycle.
    enum class ProcessState : std::uint8_t
    {
        New,        ///< Process has been created but not yet admitted to the ready queue.
        Ready,      ///< Process is waiting in the ready queue for CPU time.
        Running,    ///< Process is currently executing on the CPU.
        Blocked,    ///< Process is waiting for an event (I/O, sync primitive, etc.).
        Suspended,  ///< Process has been swapped out of main memory.
        Terminated, ///< Process has finished execution (exit or error).
    };

    /// @brief Validates whether a transition from one process state to another is legal.
    ///
    /// Legal transitions:
    /// - New        → Ready
    /// - Ready      → Running, Suspended
    /// - Running    → Ready      (preempted / time slice expired)
    /// - Running    → Blocked    (waiting for I/O or sync)
    /// - Running    → Terminated (exit or fatal error)
    /// - Blocked    → Ready      (event completed)
    /// - Blocked    → Suspended  (swapped out while blocked)
    /// - Suspended  → Ready      (swapped back in)
    ///
    /// @param from Current state.
    /// @param to   Target state.
    /// @return true if the transition is valid, false otherwise.
    [[nodiscard]] constexpr bool isValidTransition(ProcessState from, ProcessState to) noexcept
    {
        switch (from)
        {
        case ProcessState::New:
            return to == ProcessState::Ready;

        case ProcessState::Ready:
            return to == ProcessState::Running || to == ProcessState::Suspended;

        case ProcessState::Running:
            return to == ProcessState::Ready || to == ProcessState::Blocked || to == ProcessState::Terminated;

        case ProcessState::Blocked:
            return to == ProcessState::Ready || to == ProcessState::Suspended;

        case ProcessState::Suspended:
            return to == ProcessState::Ready;

        case ProcessState::Terminated:
            return false; // Terminal state — no outgoing transitions.
        }
        return false;
    }

    /// @brief Returns a human-readable name for the given process state.
    [[nodiscard]] constexpr std::string_view processStateName(ProcessState state) noexcept
    {
        switch (state)
        {
        case ProcessState::New:
            return "New";
        case ProcessState::Ready:
            return "Ready";
        case ProcessState::Running:
            return "Running";
        case ProcessState::Blocked:
            return "Blocked";
        case ProcessState::Suspended:
            return "Suspended";
        case ProcessState::Terminated:
            return "Terminated";
        }
        return "Unknown";
    }

} // namespace contur
