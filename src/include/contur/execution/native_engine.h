/// @file native_engine.h
/// @brief NativeEngine — host-OS native process execution engine.
///
/// NativeEngine is the second concrete implementation of `IExecutionEngine`
/// (alongside `InterpreterEngine`). Instead of stepping a `vector<Block>`
/// through a simulated CPU, it spawns a real host child process from the
/// path stored in `ProcessImage::nativePath()` and lets the simulator's
/// scheduler drive its lifecycle through suspend/resume primitives.
///
/// Phase 15 ships the Windows x86 backend only. The class layout reserves
/// room for a POSIX backend later without changing the public header.

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "contur/execution/i_execution_engine.h"

namespace contur {

    class ITracer;

    /// @brief Native host-process execution engine.
    ///
    /// Reads `ProcessImage::nativePath()` on first dispatch for a given PID,
    /// spawns the host binary in a suspended state, and resumes / suspends
    /// the primary thread for each `execute(...)` burst. Stdout is captured
    /// via an anonymous pipe and surfaced through `ITracer`.
    ///
    /// Behaviour summary:
    /// - First `execute()` call: `CreateProcessW` (Windows) with a redirected
    ///   stdout pipe and `CREATE_SUSPENDED`. Returns `BudgetExhausted` after
    ///   running for the configured slice — even if the process completes
    ///   immediately, the next `execute()` call detects exit and returns
    ///   `ProcessExited`.
    /// - Subsequent calls: `ResumeThread` → wait `tickBudget * tickQuantumMs`
    ///   ms → `SuspendThread`. Drain stdout. Detect exit via
    ///   `WaitForSingleObject` returning `WAIT_OBJECT_0`.
    /// - `halt()`: marks the PID; the next `execute()` call (or an explicit
    ///   external sweep) calls `TerminateProcess` and returns `Halted`.
    ///
    /// NativeEngine is **non-portable by design**. On non-Windows hosts every
    /// method returns `ExecutionResult::error(...)`. The class still compiles
    /// everywhere (the Win32 implementation is gated inside the .cpp).
    class NativeEngine final : public IExecutionEngine
    {
        public:
        /// @brief Constructs a NativeEngine.
        /// @param tracer Reference to the simulator tracer (must outlive the engine).
        /// @param tickQuantumMs Wallclock milliseconds per simulation tick.
        explicit NativeEngine(ITracer &tracer, std::uint32_t tickQuantumMs = 5);

        /// @brief Destroys the engine and forcibly terminates any surviving children.
        ~NativeEngine() override;

        // Non-copyable, movable.
        NativeEngine(const NativeEngine &) = delete;
        NativeEngine &operator=(const NativeEngine &) = delete;
        NativeEngine(NativeEngine &&) noexcept;
        NativeEngine &operator=(NativeEngine &&) noexcept;

        /// @copydoc IExecutionEngine::execute
        [[nodiscard]] ExecutionResult execute(ProcessImage &process, std::size_t tickBudget) override;

        /// @copydoc IExecutionEngine::halt
        void halt(ProcessId pid) override;

        /// @copydoc IExecutionEngine::name
        [[nodiscard]] std::string_view name() const noexcept override;

        /// @brief Returns the captured stdout for a process, or an empty string when
        ///        unknown / not yet drained. Useful for tests and demos.
        [[nodiscard]] std::string capturedStdout(ProcessId pid) const;

        /// @brief Returns true when the engine still tracks @p pid (process not yet reaped).
        [[nodiscard]] bool isTracking(ProcessId pid) const noexcept;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
