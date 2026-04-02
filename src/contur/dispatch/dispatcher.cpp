/// @file dispatcher.cpp
/// @brief Dispatcher implementation.

#include "contur/dispatch/dispatcher.h"

#include <algorithm>
#include <unordered_map>

#include "contur/core/clock.h"

#include "contur/execution/execution_context.h"
#include "contur/execution/i_execution_engine.h"
#include "contur/memory/i_virtual_memory.h"
#include "contur/process/process_image.h"
#include "contur/scheduling/i_scheduler.h"

namespace contur {

    struct Dispatcher::Impl
    {
        IScheduler &scheduler;
        IExecutionEngine &engine;
        IVirtualMemory &virtualMemory;
        IClock &clock;
        std::unordered_map<ProcessId, std::unique_ptr<ProcessImage>> processes;

        Impl(IScheduler &scheduler, IExecutionEngine &engine, IVirtualMemory &virtualMemory, IClock &clock)
            : scheduler(scheduler)
            , engine(engine)
            , virtualMemory(virtualMemory)
            , clock(clock)
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
        IScheduler &scheduler, IExecutionEngine &engine, IVirtualMemory &virtualMemory, IClock &clock
    )
        : impl_(std::make_unique<Impl>(scheduler, engine, virtualMemory, clock))
    {}

    Dispatcher::~Dispatcher() = default;
    Dispatcher::Dispatcher(Dispatcher &&) noexcept = default;
    Dispatcher &Dispatcher::operator=(Dispatcher &&) noexcept = default;

    Result<void> Dispatcher::createProcess(std::unique_ptr<ProcessImage> process, Tick currentTick)
    {
        if (!process)
        {
            return Result<void>::error(ErrorCode::InvalidArgument);
        }

        ProcessId pid = process->id();
        if (pid == INVALID_PID)
        {
            return Result<void>::error(ErrorCode::InvalidPid);
        }
        if (impl_->processes.find(pid) != impl_->processes.end())
        {
            return Result<void>::error(ErrorCode::AlreadyExists);
        }

        std::size_t slotSize = std::max<std::size_t>(1, process->codeSize());
        auto alloc = impl_->virtualMemory.allocateSlot(pid, slotSize);
        if (alloc.isError())
        {
            return Result<void>::error(alloc.errorCode());
        }

        auto load = impl_->virtualMemory.loadSegment(pid, process->code());
        if (load.isError())
        {
            (void)impl_->virtualMemory.freeSlot(pid);
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
            return enq;
        }

        impl_->processes.emplace(pid, std::move(process));
        return Result<void>::ok();
    }

    Result<void> Dispatcher::dispatch(std::size_t tickBudget)
    {
        if (impl_->processes.empty())
        {
            return Result<void>::error(ErrorCode::NotFound);
        }

        auto selected = impl_->scheduler.selectNext(impl_->clock);
        if (selected.isError())
        {
            return Result<void>::error(selected.errorCode());
        }

        ProcessId pid = selected.value();
        auto processIt = impl_->processes.find(pid);
        if (processIt == impl_->processes.end())
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        ExecutionResult result = impl_->engine.execute(*processIt->second, tickBudget);
        impl_->advanceClock(result.ticksConsumed);

        switch (result.reason)
        {
        case StopReason::BudgetExhausted:
            return Result<void>::ok();

        case StopReason::Interrupted:
            return impl_->scheduler.blockProcess(pid, impl_->clock.now());

        case StopReason::ProcessExited:
        case StopReason::Error:
        case StopReason::Halted: {
            auto terminated = impl_->scheduler.terminate(pid, impl_->clock.now());
            if (terminated.isError())
            {
                return terminated;
            }
            (void)impl_->virtualMemory.freeSlot(pid);
            impl_->processes.erase(pid);
            return Result<void>::ok();
        }
        }

        return Result<void>::error(ErrorCode::InvalidState);
    }

    Result<void> Dispatcher::terminateProcess(ProcessId pid, Tick currentTick)
    {
        auto processIt = impl_->processes.find(pid);
        if (processIt == impl_->processes.end())
        {
            return Result<void>::error(ErrorCode::NotFound);
        }

        impl_->engine.halt(pid);

        auto removed = impl_->scheduler.dequeue(pid);
        if (removed.isError() && removed.errorCode() != ErrorCode::NotFound)
        {
            return removed;
        }

        (void)processIt->second->pcb().setState(ProcessState::Terminated, currentTick);

        (void)impl_->virtualMemory.freeSlot(pid);
        impl_->processes.erase(processIt);
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
