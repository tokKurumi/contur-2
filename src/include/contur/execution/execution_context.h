/// @file execution_context.h
/// @brief ExecutionResult and related types for execution engine outcomes.
///
/// ExecutionResult is returned by IExecutionEngine::execute() to describe
/// what happened during a burst of execution: how many ticks were consumed,
/// why execution stopped, and what interrupt (if any) was raised.

#pragma once

#include <cstddef>

#include "contur/core/types.h"

#include "contur/arch/interrupt.h"

namespace contur {

    /// @brief Reason why the execution engine returned control to the dispatcher.
    enum class StopReason : std::uint8_t
    {
        BudgetExhausted, ///< Tick budget ran out (preemption / time slice expired).
        ProcessExited,   ///< Process executed Halt or Int Exit — normal termination.
        Error,           ///< An unrecoverable error occurred (invalid instruction, segfault, etc.).
        Interrupted,     ///< A software/hardware interrupt requires kernel attention (syscall, I/O, page fault).
        Halted,          ///< Process was forcibly halted via halt().
    };

    /// @brief Result of an execution burst returned by IExecutionEngine::execute().
    ///
    /// The dispatcher inspects this to decide how to transition the process:
    /// - BudgetExhausted → move to Ready (preempted)
    /// - ProcessExited   → move to Terminated
    /// - Error           → move to Terminated (with error)
    /// - Interrupted     → handle the interrupt (syscall dispatch, I/O, etc.)
    /// - Halted          → move to Terminated (forced)
    struct ExecutionResult
    {
        StopReason reason = StopReason::Error; ///< Why execution stopped.
        Interrupt interrupt = Interrupt::Ok;   ///< The interrupt that caused the stop (if Interrupted).
        std::size_t ticksConsumed = 0;         ///< Number of ticks actually executed.
        ProcessId pid = INVALID_PID;           ///< The process that was executing.

        /// @brief Creates a result for budget exhaustion (preemption).
        [[nodiscard]] static constexpr ExecutionResult budgetExhausted(ProcessId pid, std::size_t ticks) noexcept
        {
            return ExecutionResult{StopReason::BudgetExhausted, Interrupt::Ok, ticks, pid};
        }

        /// @brief Creates a result for normal process exit.
        [[nodiscard]] static constexpr ExecutionResult exited(ProcessId pid, std::size_t ticks) noexcept
        {
            return ExecutionResult{StopReason::ProcessExited, Interrupt::Exit, ticks, pid};
        }

        /// @brief Creates a result for an error condition.
        [[nodiscard]] static constexpr ExecutionResult error(ProcessId pid, std::size_t ticks, Interrupt intr) noexcept
        {
            return ExecutionResult{StopReason::Error, intr, ticks, pid};
        }

        /// @brief Creates a result for an interrupt requiring kernel attention.
        [[nodiscard]] static constexpr ExecutionResult
        interrupted(ProcessId pid, std::size_t ticks, Interrupt intr) noexcept
        {
            return ExecutionResult{StopReason::Interrupted, intr, ticks, pid};
        }

        /// @brief Creates a result for a forcibly halted process.
        [[nodiscard]] static constexpr ExecutionResult halted(ProcessId pid, std::size_t ticks) noexcept
        {
            return ExecutionResult{StopReason::Halted, Interrupt::Exit, ticks, pid};
        }
    };

} // namespace contur
