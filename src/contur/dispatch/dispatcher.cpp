/// @file dispatcher.cpp
/// @brief Dispatcher implementation.

#include "contur/dispatch/dispatcher.h"

#include <algorithm>
#include <string>
#include <unordered_map>

#include "contur/core/clock.h"

#include "contur/execution/execution_context.h"
#include "contur/execution/i_execution_engine.h"
#include "contur/memory/i_virtual_memory.h"
#include "contur/process/process_image.h"
#include "contur/scheduling/i_scheduler.h"
#include "contur/tracing/i_tracer.h"
#include "contur/tracing/trace_scope.h"

namespace contur {

    struct Dispatcher::Impl
    {
        IScheduler &scheduler;
        IExecutionEngine &engine;
        IVirtualMemory &virtualMemory;
        IClock &clock;
        ITracer &tracer;
        std::unordered_map<ProcessId, std::unique_ptr<ProcessImage>> processes;

        Impl(
            IScheduler &scheduler,
            IExecutionEngine &engine,
            IVirtualMemory &virtualMemory,
            IClock &clock,
            ITracer &tracer
        )
            : scheduler(scheduler)
            , engine(engine)
            , virtualMemory(virtualMemory)
            , clock(clock)
            , tracer(tracer)
        {}

        void advanceClock(std::size_t ticks)
        {
            for (std::size_t i = 0; i < ticks; ++i)
            {
                clock.tick();
            }
        }
    };

    Dispatcher::Dispatcher(
        IScheduler &scheduler, IExecutionEngine &engine, IVirtualMemory &virtualMemory, IClock &clock, ITracer &tracer
    )
        : impl_(std::make_unique<Impl>(scheduler, engine, virtualMemory, clock, tracer))
    {}

    Dispatcher::~Dispatcher() = default;
    Dispatcher::Dispatcher(Dispatcher &&) noexcept = default;
    Dispatcher &Dispatcher::operator=(Dispatcher &&) noexcept = default;

    Result<void> Dispatcher::createProcess(std::unique_ptr<ProcessImage> process, Tick currentTick)
    {
        CONTUR_TRACE_SCOPE(impl_->tracer, "Dispatcher", "createProcess");

        if (!process)
        {
            CONTUR_TRACE_L(
                impl_->tracer,
                TraceLevel::Warn,
                "Dispatcher",
                "createProcess.error",
                errorCodeToString(ErrorCode::InvalidArgument)
            );
            return Result<void>::error(ErrorCode::InvalidArgument);
        }

        ProcessId pid = process->id();
        if (pid == INVALID_PID)
        {
            CONTUR_TRACE_L(
                impl_->tracer,
                TraceLevel::Warn,
                "Dispatcher",
                "createProcess.error",
                errorCodeToString(ErrorCode::InvalidPid)
            );
            return Result<void>::error(ErrorCode::InvalidPid);
        }
        if (impl_->processes.find(pid) != impl_->processes.end())
        {
            CONTUR_TRACE_L(
                impl_->tracer,
                TraceLevel::Warn,
                "Dispatcher",
                "createProcess.error",
                errorCodeToString(ErrorCode::AlreadyExists)
            );
            return Result<void>::error(ErrorCode::AlreadyExists);
        }

        std::size_t slotSize = std::max<std::size_t>(1, process->codeSize());
        auto alloc = impl_->virtualMemory.allocateSlot(pid, slotSize);
        if (alloc.isError())
        {
            CONTUR_TRACE_L(
                impl_->tracer,
                TraceLevel::Error,
                "Dispatcher",
                "createProcess.error",
                errorCodeToString(alloc.errorCode())
            );
            return Result<void>::error(alloc.errorCode());
        }

        auto load = impl_->virtualMemory.loadSegment(pid, process->code());
        if (load.isError())
        {
            (void)impl_->virtualMemory.freeSlot(pid);
            CONTUR_TRACE_L(
                impl_->tracer,
                TraceLevel::Error,
                "Dispatcher",
                "createProcess.error",
                errorCodeToString(load.errorCode())
            );
            return Result<void>::error(load.errorCode());
        }

        process->pcb().addressInfo().virtualBase = alloc.value();
        process->pcb().addressInfo().codeStart = alloc.value();
        process->pcb().addressInfo().codeSize = static_cast<std::uint32_t>(process->codeSize());
        process->pcb().addressInfo().slot = static_cast<std::uint32_t>(alloc.value());

        auto enq = impl_->scheduler.enqueue(process->pcb(), currentTick);
        if (enq.isError())
        {
            (void)impl_->virtualMemory.freeSlot(pid);
            CONTUR_TRACE_L(
                impl_->tracer,
                TraceLevel::Error,
                "Dispatcher",
                "createProcess.error",
                errorCodeToString(enq.errorCode())
            );
            return enq;
        }

        impl_->processes.emplace(pid, std::move(process));
        CONTUR_TRACE(impl_->tracer, "Dispatcher", "createProcess.ok", std::string("pid=") + std::to_string(pid));
        return Result<void>::ok();
    }

    Result<void> Dispatcher::dispatch(std::size_t tickBudget)
    {
        CONTUR_TRACE_SCOPE(impl_->tracer, "Dispatcher", "dispatch");
        CONTUR_TRACE(
            impl_->tracer, "Dispatcher", "dispatch.request", std::string("budget=") + std::to_string(tickBudget)
        );

        if (impl_->processes.empty())
        {
            CONTUR_TRACE_L(impl_->tracer, TraceLevel::Debug, "Dispatcher", "dispatch.idle", "no-processes");
            return Result<void>::error(ErrorCode::NotFound);
        }

        auto selected = impl_->scheduler.selectNext(impl_->clock);
        if (selected.isError())
        {
            CONTUR_TRACE_L(
                impl_->tracer, TraceLevel::Warn, "Dispatcher", "dispatch.error", errorCodeToString(selected.errorCode())
            );
            return Result<void>::error(selected.errorCode());
        }

        ProcessId pid = selected.value();
        auto processIt = impl_->processes.find(pid);
        if (processIt == impl_->processes.end())
        {
            CONTUR_TRACE_L(
                impl_->tracer,
                TraceLevel::Error,
                "Dispatcher",
                "dispatch.error",
                errorCodeToString(ErrorCode::InvalidState)
            );
            return Result<void>::error(ErrorCode::InvalidState);
        }

        CONTUR_TRACE(impl_->tracer, "Dispatcher", "dispatch.selected", std::string("pid=") + std::to_string(pid));

        ExecutionResult result = impl_->engine.execute(*processIt->second, tickBudget);
        impl_->advanceClock(result.ticksConsumed);

        switch (result.reason)
        {
        case StopReason::BudgetExhausted:
            CONTUR_TRACE_L(impl_->tracer, TraceLevel::Debug, "Dispatcher", "dispatch.stop", "budget-exhausted");
            return Result<void>::ok();

        case StopReason::Interrupted:
            CONTUR_TRACE(impl_->tracer, "Dispatcher", "dispatch.stop", "interrupted");
            return impl_->scheduler.blockProcess(pid, impl_->clock.now());

        case StopReason::ProcessExited:
        case StopReason::Error:
        case StopReason::Halted: {
            CONTUR_TRACE(impl_->tracer, "Dispatcher", "dispatch.stop", "terminated");
            auto terminated = impl_->scheduler.terminate(pid, impl_->clock.now());
            if (terminated.isError())
            {
                CONTUR_TRACE_L(
                    impl_->tracer,
                    TraceLevel::Error,
                    "Dispatcher",
                    "dispatch.error",
                    errorCodeToString(terminated.errorCode())
                );
                return terminated;
            }
            (void)impl_->virtualMemory.freeSlot(pid);
            impl_->processes.erase(pid);
            return Result<void>::ok();
        }
        }

        CONTUR_TRACE_L(
            impl_->tracer, TraceLevel::Error, "Dispatcher", "dispatch.error", errorCodeToString(ErrorCode::InvalidState)
        );
        return Result<void>::error(ErrorCode::InvalidState);
    }

    Result<void> Dispatcher::terminateProcess(ProcessId pid, Tick currentTick)
    {
        CONTUR_TRACE_SCOPE(impl_->tracer, "Dispatcher", "terminateProcess");
        CONTUR_TRACE(
            impl_->tracer, "Dispatcher", "terminateProcess.request", std::string("pid=") + std::to_string(pid)
        );

        auto processIt = impl_->processes.find(pid);
        if (processIt == impl_->processes.end())
        {
            CONTUR_TRACE_L(
                impl_->tracer,
                TraceLevel::Warn,
                "Dispatcher",
                "terminateProcess.error",
                errorCodeToString(ErrorCode::NotFound)
            );
            return Result<void>::error(ErrorCode::NotFound);
        }

        impl_->engine.halt(pid);

        auto removed = impl_->scheduler.dequeue(pid);
        if (removed.isError() && removed.errorCode() != ErrorCode::NotFound)
        {
            CONTUR_TRACE_L(
                impl_->tracer,
                TraceLevel::Error,
                "Dispatcher",
                "terminateProcess.error",
                errorCodeToString(removed.errorCode())
            );
            return removed;
        }

        (void)processIt->second->pcb().setState(ProcessState::Terminated, currentTick);

        (void)impl_->virtualMemory.freeSlot(pid);
        impl_->processes.erase(processIt);
        CONTUR_TRACE(impl_->tracer, "Dispatcher", "terminateProcess.ok", std::string("pid=") + std::to_string(pid));
        return Result<void>::ok();
    }

    void Dispatcher::tick()
    {
        impl_->clock.tick();
    }

    std::size_t Dispatcher::processCount() const noexcept
    {
        return impl_->processes.size();
    }

    bool Dispatcher::hasProcess(ProcessId pid) const noexcept
    {
        return impl_->processes.find(pid) != impl_->processes.end();
    }

} // namespace contur
