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
    class ITracer;
    class IFileSystem;
    class IDispatchRuntime;
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

        /// @brief Tracer used for kernel-level event instrumentation.
        std::unique_ptr<ITracer> tracer;

        /// @brief File system service.
        std::unique_ptr<IFileSystem> fileSystem;

        /// @brief IPC manager service.
        std::unique_ptr<IpcManager> ipcManager;

        /// @brief Syscall dispatch table.
        std::unique_ptr<SyscallTable> syscallTable;

        /// @brief Optional dispatch runtime whose lifetime is managed by the kernel.
        /// @details When set, the kernel keeps the runtime alive for as long as the dispatcher needs it.
        std::unique_ptr<IDispatchRuntime> runtime;

        /// @brief Default per-dispatch tick budget used when caller passes 0.
        std::size_t defaultTickBudget = static_cast<std::size_t>(DEFAULT_TIME_SLICE);
    };

    /// @brief Concrete kernel facade.
    class Kernel final : public IKernel
    {
        public:
        /// @brief Constructs kernel facade from fully prepared dependencies.
        /// @param deps Owned dependency bundle assembled by KernelBuilder.
        explicit Kernel(KernelDependencies deps);

        /// @brief Destroys kernel facade and owned subsystem graph.
        ~Kernel() override;

        /// @brief Copy construction is disabled.
        Kernel(const Kernel &) = delete;

        /// @brief Copy assignment is disabled.
        Kernel &operator=(const Kernel &) = delete;
        /// @brief Move-constructs kernel facade.
        Kernel(Kernel &&) noexcept;

        /// @brief Move-assigns kernel facade.
        Kernel &operator=(Kernel &&) noexcept;

        /// @copydoc IKernel::createProcess
        [[nodiscard]] Result<ProcessId> createProcess(const ProcessConfig &config) override;

        /// @copydoc IKernel::terminateProcess
        [[nodiscard]] Result<void> terminateProcess(ProcessId pid) override;

        /// @copydoc IKernel::tick
        [[nodiscard]] Result<void> tick(std::size_t tickBudget = 0) override;

        /// @copydoc IKernel::runForTicks
        [[nodiscard]] Result<void> runForTicks(std::size_t cycles, std::size_t tickBudget = 0) override;

        /// @copydoc IKernel::syscall
        [[nodiscard]] Result<RegisterValue>
        syscall(ProcessId pid, SyscallId id, std::span<const RegisterValue> args) override;

        /// @copydoc IKernel::registerSyscallHandler
        [[nodiscard]] Result<void> registerSyscallHandler(SyscallId id, SyscallHandlerFn handler) override;

        /// @copydoc IKernel::registerSyncPrimitive
        [[nodiscard]] Result<void>
        registerSyncPrimitive(const std::string &name, std::unique_ptr<ISyncPrimitive> primitive) override;

        /// @copydoc IKernel::enterCritical
        [[nodiscard]] Result<void> enterCritical(ProcessId pid, std::string_view primitiveName) override;

        /// @copydoc IKernel::leaveCritical
        [[nodiscard]] Result<void> leaveCritical(ProcessId pid, std::string_view primitiveName) override;

        /// @copydoc IKernel::snapshot
        [[nodiscard]] KernelSnapshot snapshot() const override;

        /// @copydoc IKernel::now
        [[nodiscard]] Tick now() const noexcept override;

        /// @copydoc IKernel::hasProcess
        [[nodiscard]] bool hasProcess(ProcessId pid) const noexcept override;

        /// @copydoc IKernel::processCount
        [[nodiscard]] std::size_t processCount() const noexcept override;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
