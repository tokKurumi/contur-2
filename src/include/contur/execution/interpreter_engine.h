/// @file interpreter_engine.h
/// @brief InterpreterEngine — bytecode interpreter execution engine.
///
/// The InterpreterEngine loads a process's code segment into simulated memory,
/// then executes it step-by-step through the CPU's fetch-decode-execute cycle.
/// It implements the IExecutionEngine interface (Strategy pattern) and is
/// interchangeable with NativeEngine at runtime.

#pragma once

#include <memory>

#include "contur/execution/i_execution_engine.h"

namespace contur {

    class ICPU;
    class IMemory;

    /// @brief Bytecode interpreter execution engine.
    ///
    /// Wraps an ICPU and IMemory, loads the process's code segment into memory
    /// before each execution burst, and steps instruction-by-instruction up to
    /// the given tick budget. Reports results via ExecutionResult.
    ///
    /// Usage:
    /// @code
    /// PhysicalMemory memory(256);
    /// Cpu cpu(memory);
    /// InterpreterEngine engine(cpu, memory);
    ///
    /// auto result = engine.execute(process, 10);
    /// if (result.reason == StopReason::ProcessExited) { /* done */ }
    /// @endcode
    class InterpreterEngine final : public IExecutionEngine
    {
      public:
        /// @brief Constructs an InterpreterEngine.
        /// @param cpu    Reference to the CPU (must outlive the engine).
        /// @param memory Reference to the memory subsystem (must outlive the engine).
        explicit InterpreterEngine(ICPU &cpu, IMemory &memory);

        ~InterpreterEngine() override;

        // Non-copyable, movable
        InterpreterEngine(const InterpreterEngine &) = delete;
        InterpreterEngine &operator=(const InterpreterEngine &) = delete;
        InterpreterEngine(InterpreterEngine &&) noexcept;
        InterpreterEngine &operator=(InterpreterEngine &&) noexcept;

        /// @copydoc IExecutionEngine::execute
        [[nodiscard]] ExecutionResult execute(ProcessImage &process, std::size_t tickBudget) override;

        /// @copydoc IExecutionEngine::halt
        void halt(ProcessId pid) override;

        /// @copydoc IExecutionEngine::name
        [[nodiscard]] std::string_view name() const noexcept override;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
