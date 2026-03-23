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
    class ISchedulingPolicy;
    class IDispatcher;
    class IFileSystem;
    class IPageReplacementPolicy;
    class IpcManager;
    class SyscallTable;
    struct KernelDependencies;

    /// @brief Fluent builder that assembles Kernel dependencies.
    class KernelBuilder
    {
        public:
        KernelBuilder();
        ~KernelBuilder();

        KernelBuilder(const KernelBuilder &) = delete;
        KernelBuilder &operator=(const KernelBuilder &) = delete;
        KernelBuilder(KernelBuilder &&) noexcept;
        KernelBuilder &operator=(KernelBuilder &&) noexcept;

        [[nodiscard]] KernelBuilder &withClock(std::unique_ptr<IClock> clock);
        [[nodiscard]] KernelBuilder &withMemory(std::unique_ptr<IMemory> memory);
        [[nodiscard]] KernelBuilder &withMmu(std::unique_ptr<IMMU> mmu);
        [[nodiscard]] KernelBuilder &withVirtualMemory(std::unique_ptr<IVirtualMemory> virtualMemory);
        [[nodiscard]] KernelBuilder &withCpu(std::unique_ptr<ICPU> cpu);
        [[nodiscard]] KernelBuilder &withExecutionEngine(std::unique_ptr<IExecutionEngine> executionEngine);
        [[nodiscard]] KernelBuilder &withSchedulingPolicy(std::unique_ptr<ISchedulingPolicy> policy);
        [[nodiscard]] KernelBuilder &withScheduler(std::unique_ptr<IScheduler> scheduler);
        [[nodiscard]] KernelBuilder &withDispatcher(std::unique_ptr<IDispatcher> dispatcher);
        [[nodiscard]] KernelBuilder &withFileSystem(std::unique_ptr<IFileSystem> fileSystem);
        [[nodiscard]] KernelBuilder &withIpcManager(std::unique_ptr<IpcManager> ipcManager);
        [[nodiscard]] KernelBuilder &withSyscallTable(std::unique_ptr<SyscallTable> syscallTable);
        [[nodiscard]] KernelBuilder &withPageReplacementPolicy(std::unique_ptr<IPageReplacementPolicy> replacement);
        [[nodiscard]] KernelBuilder &withDefaultTickBudget(std::size_t ticks);

        /// @brief Builds a fully wired kernel instance.
        [[nodiscard]] std::unique_ptr<IKernel> build();

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
