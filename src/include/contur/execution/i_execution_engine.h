/// @file i_execution_engine.h
/// @brief IExecutionEngine — abstract interface for process execution engines.
///
/// Both the bytecode interpreter and the native process manager implement
/// this interface. The dispatcher uses it polymorphically (Strategy pattern)
/// to run processes without caring about the underlying execution mechanism.

#pragma once

#include <cstddef>
#include <string_view>

#include "contur/core/types.h"

#include "contur/execution/execution_context.h"

namespace contur {

    class ProcessImage;

    /// @brief Abstract execution engine interface.
    ///
    /// An execution engine takes a ProcessImage and runs it for up to
    /// @p tickBudget simulation ticks, returning an ExecutionResult that
    /// describes what happened (how many ticks were consumed, why execution
    /// stopped, etc.).
    ///
    /// Implementations:
    /// - **InterpreterEngine** — fetches Block instructions from memory,
    ///   decodes via CPU, executes step-by-step (educational, fully portable)
    /// - **NativeEngine** — wraps host OS process management (`fork/exec`
    ///   on POSIX, `CreateProcess` on Windows)
    class IExecutionEngine
    {
      public:
        virtual ~IExecutionEngine() = default;

        /// @brief Executes the given process for up to @p tickBudget ticks.
        ///
        /// The engine runs the process until one of:
        /// - The tick budget is exhausted (preemption)
        /// - The process exits (Halt / Int Exit)
        /// - An error or interrupt occurs that requires kernel attention
        ///
        /// @param process   The process image to execute (registers + code).
        /// @param tickBudget Maximum number of ticks (instructions) to execute.
        /// @return An ExecutionResult describing the outcome.
        [[nodiscard]] virtual ExecutionResult execute(ProcessImage &process, std::size_t tickBudget) = 0;

        /// @brief Forcibly halts execution of the given process.
        /// @param pid The process ID to halt.
        virtual void halt(ProcessId pid) = 0;

        /// @brief Returns the human-readable name of this engine.
        /// @return "Interpreter" or "Native", etc.
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;

      protected:
        IExecutionEngine() = default;
        IExecutionEngine(const IExecutionEngine &) = default;
        IExecutionEngine &operator=(const IExecutionEngine &) = default;
        IExecutionEngine(IExecutionEngine &&) = default;
        IExecutionEngine &operator=(IExecutionEngine &&) = default;
    };

} // namespace contur
