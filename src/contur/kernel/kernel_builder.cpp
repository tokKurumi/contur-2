/// @file kernel_builder.cpp
/// @brief KernelBuilder implementation.

#include "contur/kernel/kernel_builder.h"

#include <utility>

#include "contur/core/clock.h"

#include "contur/cpu/i_cpu.h"
#include "contur/dispatch/i_dispatch_runtime.h"
#include "contur/dispatch/i_dispatcher.h"
#include "contur/execution/i_execution_engine.h"
#include "contur/fs/i_filesystem.h"
#include "contur/ipc/ipc_manager.h"
#include "contur/kernel/kernel.h"
#include "contur/memory/i_memory.h"
#include "contur/memory/i_mmu.h"
#include "contur/memory/i_virtual_memory.h"
#include "contur/scheduling/i_scheduler.h"
#include "contur/syscall/syscall_table.h"
#include "contur/tracing/i_tracer.h"

namespace contur {

    namespace {

        [[nodiscard]] bool hasAllRequiredDependencies(const KernelDependencies &deps)
        {
            return deps.clock && deps.memory && deps.mmu && deps.virtualMemory && deps.cpu && deps.executionEngine &&
                   deps.scheduler && deps.dispatcher && deps.tracer && deps.fileSystem && deps.ipcManager &&
                   deps.syscallTable;
        }

    } // namespace

    struct KernelBuilder::Impl
    {
        KernelDependencies deps;
    };

    KernelBuilder::KernelBuilder()
        : impl_(std::make_unique<Impl>())
    {
        impl_->deps.defaultTickBudget = 0;
    }

    KernelBuilder::~KernelBuilder() = default;
    KernelBuilder::KernelBuilder(KernelBuilder &&) noexcept = default;
    KernelBuilder &KernelBuilder::operator=(KernelBuilder &&) noexcept = default;

    KernelBuilder &KernelBuilder::withClock(std::unique_ptr<IClock> clock)
    {
        impl_->deps.clock = std::move(clock);
        return *this;
    }

    KernelBuilder &KernelBuilder::withMemory(std::unique_ptr<IMemory> memory)
    {
        impl_->deps.memory = std::move(memory);
        return *this;
    }

    KernelBuilder &KernelBuilder::withMmu(std::unique_ptr<IMMU> mmu)
    {
        impl_->deps.mmu = std::move(mmu);
        return *this;
    }

    KernelBuilder &KernelBuilder::withVirtualMemory(std::unique_ptr<IVirtualMemory> virtualMemory)
    {
        impl_->deps.virtualMemory = std::move(virtualMemory);
        return *this;
    }

    KernelBuilder &KernelBuilder::withCpu(std::unique_ptr<ICPU> cpu)
    {
        impl_->deps.cpu = std::move(cpu);
        return *this;
    }

    KernelBuilder &KernelBuilder::withExecutionEngine(std::unique_ptr<IExecutionEngine> executionEngine)
    {
        impl_->deps.executionEngine = std::move(executionEngine);
        return *this;
    }

    KernelBuilder &KernelBuilder::withScheduler(std::unique_ptr<IScheduler> scheduler)
    {
        impl_->deps.scheduler = std::move(scheduler);
        return *this;
    }

    KernelBuilder &KernelBuilder::withDispatcher(std::unique_ptr<IDispatcher> dispatcher)
    {
        impl_->deps.dispatcher = std::move(dispatcher);
        return *this;
    }

    KernelBuilder &KernelBuilder::withTracer(std::unique_ptr<ITracer> tracer)
    {
        impl_->deps.tracer = std::move(tracer);
        return *this;
    }

    KernelBuilder &KernelBuilder::withRuntime(std::unique_ptr<IDispatchRuntime> runtime)
    {
        impl_->deps.runtime = std::move(runtime);
        return *this;
    }

    KernelBuilder &KernelBuilder::withFileSystem(std::unique_ptr<IFileSystem> fileSystem)
    {
        impl_->deps.fileSystem = std::move(fileSystem);
        return *this;
    }

    KernelBuilder &KernelBuilder::withIpcManager(std::unique_ptr<IpcManager> ipcManager)
    {
        impl_->deps.ipcManager = std::move(ipcManager);
        return *this;
    }

    KernelBuilder &KernelBuilder::withSyscallTable(std::unique_ptr<SyscallTable> syscallTable)
    {
        impl_->deps.syscallTable = std::move(syscallTable);
        return *this;
    }

    KernelBuilder &KernelBuilder::withDefaultTickBudget(std::size_t ticks)
    {
        impl_->deps.defaultTickBudget = ticks;
        return *this;
    }

    Result<std::unique_ptr<IKernel>> KernelBuilder::build()
    {
        if (!hasAllRequiredDependencies(impl_->deps))
        {
            return Result<std::unique_ptr<IKernel>>::error(ErrorCode::InvalidState);
        }

        if (impl_->deps.defaultTickBudget == 0)
        {
            return Result<std::unique_ptr<IKernel>>::error(ErrorCode::InvalidArgument);
        }

        return Result<std::unique_ptr<IKernel>>::ok(std::make_unique<Kernel>(std::move(impl_->deps)));
    }

} // namespace contur
