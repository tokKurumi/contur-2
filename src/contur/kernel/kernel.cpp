/// @file kernel.cpp
/// @brief Kernel facade implementation.

#include "contur/kernel/kernel.h"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <utility>

#include "contur/core/clock.h"

#include "contur/cpu/i_cpu.h"
#include "contur/dispatch/i_dispatch_runtime.h"
#include "contur/dispatch/i_dispatcher.h"
#include "contur/execution/i_execution_engine.h"
#include "contur/fs/i_filesystem.h"
#include "contur/ipc/ipc_manager.h"
#include "contur/memory/i_memory.h"
#include "contur/memory/i_mmu.h"
#include "contur/memory/i_virtual_memory.h"
#include "contur/process/process_image.h"
#include "contur/scheduling/i_scheduler.h"
#include "contur/sync/i_sync_primitive.h"
#include "contur/syscall/syscall_table.h"
#include "contur/tracing/i_tracer.h"
#include "contur/tracing/trace_scope.h"

namespace contur {

    struct Kernel::Impl
    {
        std::unique_ptr<IClock> clock;
        std::unique_ptr<IMemory> memory;
        std::unique_ptr<IMMU> mmu;
        std::unique_ptr<IVirtualMemory> virtualMemory;
        std::unique_ptr<ICPU> cpu;
        std::unique_ptr<IExecutionEngine> executionEngine;
        std::unique_ptr<IScheduler> scheduler;
        std::unique_ptr<IDispatcher> dispatcher;
        std::unique_ptr<ITracer> tracer;
        std::unique_ptr<IDispatchRuntime> runtime;
        std::unique_ptr<IFileSystem> fileSystem;
        std::unique_ptr<IpcManager> ipcManager;
        std::unique_ptr<SyscallTable> syscallTable;
        std::unordered_map<std::string, std::unique_ptr<ISyncPrimitive>> syncPrimitives;
        std::unordered_map<ProcessId, std::reference_wrapper<ProcessImage>> processRefs;
        ProcessId nextPid = 1;
        std::size_t defaultTickBudget = static_cast<std::size_t>(DEFAULT_TIME_SLICE);

        explicit Impl(KernelDependencies deps)
            : clock(std::move(deps.clock))
            , memory(std::move(deps.memory))
            , mmu(std::move(deps.mmu))
            , virtualMemory(std::move(deps.virtualMemory))
            , cpu(std::move(deps.cpu))
            , executionEngine(std::move(deps.executionEngine))
            , scheduler(std::move(deps.scheduler))
            , dispatcher(std::move(deps.dispatcher))
            , tracer(std::move(deps.tracer))
            , runtime(std::move(deps.runtime))
            , fileSystem(std::move(deps.fileSystem))
            , ipcManager(std::move(deps.ipcManager))
            , syscallTable(std::move(deps.syscallTable))
            , defaultTickBudget(deps.defaultTickBudget)
        {}

        [[nodiscard]] bool processExists(ProcessId pid) const noexcept
        {
            return dispatcher && dispatcher->hasProcess(pid);
        }

        void pruneDeadProcessRefs()
        {
            for (auto it = processRefs.begin(); it != processRefs.end();)
            {
                if (!processExists(it->first))
                {
                    it = processRefs.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        [[nodiscard]] ProcessId allocatePid()
        {
            if (!dispatcher)
            {
                return INVALID_PID;
            }

            for (std::size_t i = 0; i < static_cast<std::size_t>(MAX_PROCESSES); ++i)
            {
                if (nextPid == INVALID_PID)
                {
                    nextPid = 1;
                }

                ProcessId candidate = nextPid;
                ++nextPid;
                if (!dispatcher->hasProcess(candidate))
                {
                    return candidate;
                }
            }

            return INVALID_PID;
        }
    };

    Kernel::Kernel(KernelDependencies deps)
        : impl_(std::make_unique<Impl>(std::move(deps)))
    {}

    Kernel::~Kernel() = default;
    Kernel::Kernel(Kernel &&) noexcept = default;
    Kernel &Kernel::operator=(Kernel &&) noexcept = default;

    Result<ProcessId> Kernel::createProcess(const ProcessConfig &config)
    {
        if (!impl_->dispatcher || !impl_->clock || !impl_->tracer)
        {
            return Result<ProcessId>::error(ErrorCode::InvalidState);
        }

        CONTUR_TRACE_SCOPE(*impl_->tracer, "Kernel", "createProcess");

        if (config.code.empty())
        {
            CONTUR_TRACE(
                *impl_->tracer, "Kernel", "createProcess.error", errorCodeToString(ErrorCode::InvalidArgument)
            );
            return Result<ProcessId>::error(ErrorCode::InvalidArgument);
        }

        ProcessId pid = config.id;
        if (pid == INVALID_PID)
        {
            pid = impl_->allocatePid();
            if (pid == INVALID_PID)
            {
                CONTUR_TRACE(
                    *impl_->tracer, "Kernel", "createProcess.error", errorCodeToString(ErrorCode::OutOfMemory)
                );
                return Result<ProcessId>::error(ErrorCode::OutOfMemory);
            }
        }
        else if (impl_->dispatcher->hasProcess(pid))
        {
            CONTUR_TRACE(*impl_->tracer, "Kernel", "createProcess.error", errorCodeToString(ErrorCode::AlreadyExists));
            return Result<ProcessId>::error(ErrorCode::AlreadyExists);
        }

        std::string processName = config.name.empty() ? ("process-" + std::to_string(pid)) : config.name;
        Tick arrival = config.arrivalTime == 0 ? impl_->clock->now() : config.arrivalTime;

        auto process = std::make_unique<ProcessImage>(pid, processName, config.code, config.priority, arrival);
        ProcessImage &processRef = *process;

        auto created = impl_->dispatcher->createProcess(std::move(process), impl_->clock->now());
        if (created.isError())
        {
            CONTUR_TRACE(*impl_->tracer, "Kernel", "createProcess.error", errorCodeToString(created.errorCode()));
            return Result<ProcessId>::error(created.errorCode());
        }

        impl_->processRefs.emplace(pid, processRef);
        CONTUR_TRACE(*impl_->tracer, "Kernel", "createProcess.ok", std::string("pid=") + std::to_string(pid));
        return Result<ProcessId>::ok(pid);
    }

    Result<void> Kernel::terminateProcess(ProcessId pid)
    {
        if (!impl_->dispatcher || !impl_->clock || !impl_->tracer)
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        CONTUR_TRACE_SCOPE(*impl_->tracer, "Kernel", "terminateProcess");
        CONTUR_TRACE(*impl_->tracer, "Kernel", "terminateProcess.request", std::string("pid=") + std::to_string(pid));

        auto terminated = impl_->dispatcher->terminateProcess(pid, impl_->clock->now());
        if (terminated.isError())
        {
            CONTUR_TRACE(*impl_->tracer, "Kernel", "terminateProcess.error", errorCodeToString(terminated.errorCode()));
            return terminated;
        }

        impl_->processRefs.erase(pid);
        CONTUR_TRACE(*impl_->tracer, "Kernel", "terminateProcess.ok", std::string("pid=") + std::to_string(pid));
        return Result<void>::ok();
    }

    Result<void> Kernel::tick(std::size_t tickBudget)
    {
        if (!impl_->dispatcher || !impl_->tracer)
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        CONTUR_TRACE_SCOPE(*impl_->tracer, "Kernel", "tick");

        std::size_t budget = tickBudget == 0 ? impl_->defaultTickBudget : tickBudget;
        if (budget == 0)
        {
            budget = 1;
        }

        CONTUR_TRACE(*impl_->tracer, "Kernel", "tick.dispatch", std::string("budget=") + std::to_string(budget));

        auto result = impl_->dispatcher->dispatch(budget);
        impl_->pruneDeadProcessRefs();
        if (result.isError())
        {
            CONTUR_TRACE(*impl_->tracer, "Kernel", "tick.error", errorCodeToString(result.errorCode()));
        }
        return result;
    }

    Result<void> Kernel::runForTicks(std::size_t cycles, std::size_t tickBudget)
    {
        for (std::size_t i = 0; i < cycles; ++i)
        {
            auto tickResult = tick(tickBudget);
            if (tickResult.isError())
            {
                if (tickResult.errorCode() == ErrorCode::NotFound)
                {
                    return Result<void>::ok();
                }
                return tickResult;
            }
        }
        return Result<void>::ok();
    }

    Result<RegisterValue> Kernel::syscall(ProcessId pid, SyscallId id, std::span<const RegisterValue> args)
    {
        if (!impl_->syscallTable || !impl_->dispatcher)
        {
            return Result<RegisterValue>::error(ErrorCode::InvalidState);
        }

        auto it = impl_->processRefs.find(pid);
        if (it == impl_->processRefs.end() || !impl_->dispatcher->hasProcess(pid))
        {
            impl_->processRefs.erase(pid);
            return Result<RegisterValue>::error(ErrorCode::NotFound);
        }

        return impl_->syscallTable->dispatch(id, args, it->second.get());
    }

    Result<void> Kernel::registerSyscallHandler(SyscallId id, SyscallHandlerFn handler)
    {
        if (!impl_->syscallTable)
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }
        return impl_->syscallTable->registerHandler(id, std::move(handler));
    }

    Result<void> Kernel::registerSyncPrimitive(const std::string &name, std::unique_ptr<ISyncPrimitive> primitive)
    {
        if (name.empty() || !primitive)
        {
            return Result<void>::error(ErrorCode::InvalidArgument);
        }
        if (impl_->syncPrimitives.find(name) != impl_->syncPrimitives.end())
        {
            return Result<void>::error(ErrorCode::AlreadyExists);
        }

        impl_->syncPrimitives.emplace(name, std::move(primitive));
        return Result<void>::ok();
    }

    Result<void> Kernel::enterCritical(ProcessId pid, std::string_view primitiveName)
    {
        auto it = impl_->syncPrimitives.find(std::string(primitiveName));
        if (it == impl_->syncPrimitives.end())
        {
            return Result<void>::error(ErrorCode::NotFound);
        }
        return it->second->acquire(pid);
    }

    Result<void> Kernel::leaveCritical(ProcessId pid, std::string_view primitiveName)
    {
        auto it = impl_->syncPrimitives.find(std::string(primitiveName));
        if (it == impl_->syncPrimitives.end())
        {
            return Result<void>::error(ErrorCode::NotFound);
        }
        return it->second->release(pid);
    }

    KernelSnapshot Kernel::snapshot() const
    {
        // Snapshot intentionally excludes host-runtime threading configuration.
        KernelSnapshot out;

        if (impl_->clock)
        {
            out.currentTick = impl_->clock->now();
        }

        if (impl_->dispatcher)
        {
            out.processCount = impl_->dispatcher->processCount();
        }

        if (impl_->scheduler)
        {
            out.readyQueue = impl_->scheduler->getQueueSnapshot();
            out.blockedQueue = impl_->scheduler->getBlockedSnapshot();
            out.readyCount = out.readyQueue.size();
            out.blockedCount = out.blockedQueue.size();
            out.runningPids = impl_->scheduler->runningProcesses();
            out.perLaneReadyQueues = impl_->scheduler->getPerLaneQueueSnapshot();
            out.policyName = std::string(impl_->scheduler->policyName());

            std::unordered_map<ProcessId, std::size_t> laneByPid;
            for (std::size_t lane = 0; lane < out.perLaneReadyQueues.size(); ++lane)
            {
                for (ProcessId pid : out.perLaneReadyQueues[lane])
                {
                    laneByPid[pid] = lane;
                }
            }

            out.processes.reserve(impl_->processRefs.size());
            for (const auto &[pid, processRef] : impl_->processRefs)
            {
                const ProcessImage &process = processRef.get();
                const PCB &pcb = process.pcb();

                KernelProcessSnapshot row;
                row.id = pid;
                row.name = std::string(pcb.name());
                row.state = pcb.state();
                row.basePriority = pcb.priority().base;
                row.effectivePriority = pcb.priority().effective;
                row.nice = pcb.priority().nice;
                row.cpuTime = pcb.timing().totalCpuTime;

                if (auto laneIt = laneByPid.find(pid); laneIt != laneByPid.end())
                {
                    row.laneIndex = laneIt->second;
                }

                out.processes.push_back(std::move(row));
            }

            std::sort(
                out.processes.begin(),
                out.processes.end(),
                [](const KernelProcessSnapshot &lhs, const KernelProcessSnapshot &rhs) { return lhs.id < rhs.id; }
            );
        }

        if (impl_->virtualMemory)
        {
            out.totalVirtualSlots = impl_->virtualMemory->totalSlots();
            out.freeVirtualSlots = impl_->virtualMemory->freeSlots();
        }

        return out;
    }

    Tick Kernel::now() const noexcept
    {
        if (!impl_->clock)
        {
            return 0;
        }
        return impl_->clock->now();
    }

    bool Kernel::hasProcess(ProcessId pid) const noexcept
    {
        if (!impl_->dispatcher)
        {
            return false;
        }
        return impl_->dispatcher->hasProcess(pid);
    }

    std::size_t Kernel::processCount() const noexcept
    {
        if (!impl_->dispatcher)
        {
            return 0;
        }
        return impl_->dispatcher->processCount();
    }

} // namespace contur
