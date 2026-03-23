/// @file kernel_builder.cpp
/// @brief KernelBuilder implementation.

#include "contur/kernel/kernel_builder.h"

#include <utility>

#include "contur/core/clock.h"

#include "contur/cpu/cpu.h"
#include "contur/dispatch/dispatcher.h"
#include "contur/execution/interpreter_engine.h"
#include "contur/fs/simple_fs.h"
#include "contur/ipc/ipc_manager.h"
#include "contur/kernel/kernel.h"
#include "contur/memory/fifo_replacement.h"
#include "contur/memory/mmu.h"
#include "contur/memory/physical_memory.h"
#include "contur/memory/virtual_memory.h"
#include "contur/scheduling/round_robin_policy.h"
#include "contur/scheduling/scheduler.h"
#include "contur/syscall/syscall_table.h"

namespace contur {

    struct KernelBuilder::Impl
    {
        KernelDependencies deps;
        std::unique_ptr<IPageReplacementPolicy> pageReplacementPolicy;
        std::unique_ptr<ISchedulingPolicy> schedulingPolicy;
        std::size_t defaultTickBudget = static_cast<std::size_t>(DEFAULT_TIME_SLICE);
    };

    KernelBuilder::KernelBuilder()
        : impl_(std::make_unique<Impl>())
    {}

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

    KernelBuilder &KernelBuilder::withSchedulingPolicy(std::unique_ptr<ISchedulingPolicy> policy)
    {
        impl_->schedulingPolicy = std::move(policy);
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

    KernelBuilder &KernelBuilder::withPageReplacementPolicy(std::unique_ptr<IPageReplacementPolicy> replacement)
    {
        impl_->pageReplacementPolicy = std::move(replacement);
        return *this;
    }

    KernelBuilder &KernelBuilder::withDefaultTickBudget(std::size_t ticks)
    {
        impl_->defaultTickBudget = ticks;
        return *this;
    }

    std::unique_ptr<IKernel> KernelBuilder::build()
    {
        if (!impl_->deps.clock)
        {
            impl_->deps.clock = std::make_unique<SimulationClock>();
        }

        if (!impl_->deps.memory)
        {
            impl_->deps.memory = std::make_unique<PhysicalMemory>(2048);
        }

        if (!impl_->deps.mmu)
        {
            if (!impl_->pageReplacementPolicy)
            {
                impl_->pageReplacementPolicy = std::make_unique<FifoReplacement>();
            }
            impl_->deps.mmu = std::make_unique<Mmu>(*impl_->deps.memory, std::move(impl_->pageReplacementPolicy));
        }

        if (!impl_->deps.virtualMemory)
        {
            impl_->deps.virtualMemory = std::make_unique<VirtualMemory>(*impl_->deps.mmu, MAX_PROCESSES);
        }

        if (!impl_->deps.cpu)
        {
            impl_->deps.cpu = std::make_unique<Cpu>(*impl_->deps.memory);
        }

        if (!impl_->deps.executionEngine)
        {
            impl_->deps.executionEngine = std::make_unique<InterpreterEngine>(*impl_->deps.cpu, *impl_->deps.memory);
        }

        if (!impl_->deps.scheduler)
        {
            if (!impl_->schedulingPolicy)
            {
                std::size_t rrSlice = impl_->defaultTickBudget == 0 ? static_cast<std::size_t>(DEFAULT_TIME_SLICE)
                                                                    : impl_->defaultTickBudget;
                impl_->schedulingPolicy = std::make_unique<RoundRobinPolicy>(rrSlice);
            }
            impl_->deps.scheduler = std::make_unique<Scheduler>(std::move(impl_->schedulingPolicy));
        }

        if (!impl_->deps.dispatcher)
        {
            impl_->deps.dispatcher = std::make_unique<Dispatcher>(
                *impl_->deps.scheduler, *impl_->deps.executionEngine, *impl_->deps.virtualMemory, *impl_->deps.clock
            );
        }

        if (!impl_->deps.fileSystem)
        {
            impl_->deps.fileSystem = std::make_unique<SimpleFS>();
        }

        if (!impl_->deps.ipcManager)
        {
            impl_->deps.ipcManager = std::make_unique<IpcManager>();
        }

        if (!impl_->deps.syscallTable)
        {
            impl_->deps.syscallTable = std::make_unique<SyscallTable>();
        }

        impl_->deps.defaultTickBudget =
            impl_->defaultTickBudget == 0 ? static_cast<std::size_t>(DEFAULT_TIME_SLICE) : impl_->defaultTickBudget;

        return std::make_unique<Kernel>(std::move(impl_->deps));
    }

} // namespace contur
