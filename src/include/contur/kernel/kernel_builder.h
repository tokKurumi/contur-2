/// @file kernel_builder.h
/// @brief KernelBuilder dependency-injection assembler.

#pragma once

#include <cstddef>
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
    class IDispatchRuntime;
    class ITracer;
    class IFileSystem;
    class IpcManager;
    class SyscallTable;
    struct KernelDependencies;

    /// @brief Fluent builder that assembles Kernel dependencies.
    class KernelBuilder
    {
        public:
        /// @brief Creates an empty builder with no pre-wired dependencies.
        KernelBuilder();

        /// @brief Destroys the builder and any still-owned dependency objects.
        ~KernelBuilder();

        /// @brief Copy construction is disabled.
        KernelBuilder(const KernelBuilder &) = delete;

        /// @brief Copy assignment is disabled.
        KernelBuilder &operator=(const KernelBuilder &) = delete;
        /// @brief Move-constructs builder state.
        KernelBuilder(KernelBuilder &&) noexcept;

        /// @brief Move-assigns builder state.
        KernelBuilder &operator=(KernelBuilder &&) noexcept;

        /// @brief Injects simulation clock dependency.
        /// @param clock Clock implementation owned by the builder.
        /// @return Reference to this builder.
        [[nodiscard]] KernelBuilder &withClock(std::unique_ptr<IClock> clock);

        /// @brief Injects physical memory dependency.
        /// @param memory Memory implementation owned by the builder.
        /// @return Reference to this builder.
        [[nodiscard]] KernelBuilder &withMemory(std::unique_ptr<IMemory> memory);

        /// @brief Injects MMU dependency.
        /// @param mmu MMU implementation owned by the builder.
        /// @return Reference to this builder.
        [[nodiscard]] KernelBuilder &withMmu(std::unique_ptr<IMMU> mmu);

        /// @brief Injects virtual memory dependency.
        /// @param virtualMemory Virtual memory implementation owned by the builder.
        /// @return Reference to this builder.
        [[nodiscard]] KernelBuilder &withVirtualMemory(std::unique_ptr<IVirtualMemory> virtualMemory);

        /// @brief Injects CPU dependency.
        /// @param cpu CPU implementation owned by the builder.
        /// @return Reference to this builder.
        [[nodiscard]] KernelBuilder &withCpu(std::unique_ptr<ICPU> cpu);

        /// @brief Injects execution-engine dependency.
        /// @param executionEngine Engine implementation owned by the builder.
        /// @return Reference to this builder.
        [[nodiscard]] KernelBuilder &withExecutionEngine(std::unique_ptr<IExecutionEngine> executionEngine);

        /// @brief Injects scheduler dependency.
        /// @param scheduler Scheduler implementation owned by the builder.
        /// @return Reference to this builder.
        [[nodiscard]] KernelBuilder &withScheduler(std::unique_ptr<IScheduler> scheduler);

        /// @brief Injects dispatcher dependency.
        /// @param dispatcher Dispatcher implementation owned by the builder.
        /// @return Reference to this builder.
        [[nodiscard]] KernelBuilder &withDispatcher(std::unique_ptr<IDispatcher> dispatcher);

        /// @brief Injects tracing dependency.
        /// @param tracer Tracer implementation owned by the builder.
        /// @return Reference to this builder.
        [[nodiscard]] KernelBuilder &withTracer(std::unique_ptr<ITracer> tracer);

        /// @brief Injects a dispatch runtime whose lifetime is managed by the kernel.
        /// @details Use this when the runtime must outlive all dispatchers it drives.
        /// @param runtime Runtime implementation owned by the kernel.
        /// @return Reference to this builder.
        [[nodiscard]] KernelBuilder &withRuntime(std::unique_ptr<IDispatchRuntime> runtime);

        /// @brief Injects filesystem dependency.
        /// @param fileSystem Filesystem implementation owned by the builder.
        /// @return Reference to this builder.
        [[nodiscard]] KernelBuilder &withFileSystem(std::unique_ptr<IFileSystem> fileSystem);

        /// @brief Injects IPC manager dependency.
        /// @param ipcManager IPC manager implementation owned by the builder.
        /// @return Reference to this builder.
        [[nodiscard]] KernelBuilder &withIpcManager(std::unique_ptr<IpcManager> ipcManager);

        /// @brief Injects syscall table dependency.
        /// @param syscallTable Syscall table implementation owned by the builder.
        /// @return Reference to this builder.
        [[nodiscard]] KernelBuilder &withSyscallTable(std::unique_ptr<SyscallTable> syscallTable);

        /// @brief Configures default dispatcher tick budget used by Kernel::tick(0).
        /// @param ticks Default budget in simulation ticks.
        /// @return Reference to this builder.
        [[nodiscard]] KernelBuilder &withDefaultTickBudget(std::size_t ticks);

        /// @brief Builds a kernel instance from explicitly injected dependencies.
        [[nodiscard]] Result<std::unique_ptr<IKernel>> build();

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
