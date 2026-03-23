/// @file kernel.h
/// @brief Kernel facade implementation.

#pragma once

#include <memory>

#include "contur/kernel/i_kernel.h"

namespace contur {

    class IClock;
    class IMemory;
    class IMMU;
    class IVirtualMemory;
    class ICPU;
    class IExecutionEngine;
    class IScheduler;
    class IDispatcher;
    class IFileSystem;
    class IPageReplacementPolicy;
    class ISchedulingPolicy;
    class SyscallTable;
    class IpcManager;

    /// @brief Dependency bundle used to construct a Kernel.
    struct KernelDependencies
    {
        /// @brief Simulation clock implementation.
        std::unique_ptr<IClock> clock;

        /// @brief Physical memory backend.
        std::unique_ptr<IMemory> memory;

        /// @brief Memory management unit implementation.
        std::unique_ptr<IMMU> mmu;

        /// @brief Virtual memory manager.
        std::unique_ptr<IVirtualMemory> virtualMemory;

        /// @brief CPU implementation used by execution engines.
        std::unique_ptr<ICPU> cpu;

        /// @brief Execution engine (interpreter or native).
        std::unique_ptr<IExecutionEngine> executionEngine;

        /// @brief Scheduler implementation.
        std::unique_ptr<IScheduler> scheduler;

        /// @brief Dispatcher orchestrating process lifecycle.
        std::unique_ptr<IDispatcher> dispatcher;

        /// @brief File system service.
        std::unique_ptr<IFileSystem> fileSystem;

        /// @brief IPC manager service.
        std::unique_ptr<IpcManager> ipcManager;

        /// @brief Syscall dispatch table.
        std::unique_ptr<SyscallTable> syscallTable;

        /// @brief Default per-dispatch tick budget used when caller passes 0.
        std::size_t defaultTickBudget = static_cast<std::size_t>(DEFAULT_TIME_SLICE);
    };

    /// @brief Concrete kernel facade.
    class Kernel final : public IKernel
    {
        public:
        explicit Kernel(KernelDependencies deps);
        ~Kernel() override;

        Kernel(const Kernel &) = delete;
        Kernel &operator=(const Kernel &) = delete;
        Kernel(Kernel &&) noexcept;
        Kernel &operator=(Kernel &&) noexcept;

        [[nodiscard]] Result<ProcessId> createProcess(const ProcessConfig &config) override;
        [[nodiscard]] Result<void> terminateProcess(ProcessId pid) override;
        [[nodiscard]] Result<void> tick(std::size_t tickBudget = 0) override;
        [[nodiscard]] Result<void> runForTicks(std::size_t cycles, std::size_t tickBudget = 0) override;
        [[nodiscard]] Result<RegisterValue>
        syscall(ProcessId pid, SyscallId id, std::span<const RegisterValue> args) override;
        [[nodiscard]] Result<void> registerSyscallHandler(SyscallId id, SyscallHandlerFn handler) override;
        [[nodiscard]] Result<void>
        registerSyncPrimitive(const std::string &name, std::unique_ptr<ISyncPrimitive> primitive) override;
        [[nodiscard]] Result<void> enterCritical(ProcessId pid, std::string_view primitiveName) override;
        [[nodiscard]] Result<void> leaveCritical(ProcessId pid, std::string_view primitiveName) override;
        [[nodiscard]] KernelSnapshot snapshot() const override;
        [[nodiscard]] Tick now() const noexcept override;
        [[nodiscard]] bool hasProcess(ProcessId pid) const noexcept override;
        [[nodiscard]] std::size_t processCount() const noexcept override;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
