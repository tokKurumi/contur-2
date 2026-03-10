/// @file pcb.cpp
/// @brief PCB implementation — Process Control Block.

#include "contur/process/pcb.h"

#include <cassert>
#include <utility>

namespace contur {

    struct PCB::Impl
    {
        ProcessId id;
        std::string name;
        ProcessState state = ProcessState::New;
        Priority priority;
        ProcessTiming timing;
        ProcessAddressInfo addressInfo;

        Impl(ProcessId id, std::string name, Priority priority, Tick arrivalTime)
            : id(id)
            , name(std::move(name))
            , priority(std::move(priority))
        {
            timing.arrivalTime = arrivalTime;
            timing.lastStateChange = arrivalTime;
        }
    };

    PCB::PCB(ProcessId id, std::string name, Priority priority, Tick arrivalTime)
        : impl_(std::make_unique<Impl>(id, std::move(name), std::move(priority), arrivalTime))
    {
        assert(id != INVALID_PID && "Cannot create PCB with INVALID_PID");
    }

    PCB::~PCB() = default;

    PCB::PCB(PCB &&) noexcept = default;
    PCB &PCB::operator=(PCB &&) noexcept = default;

    ProcessId PCB::id() const noexcept
    {
        return impl_->id;
    }

    std::string_view PCB::name() const noexcept
    {
        return impl_->name;
    }

    ProcessState PCB::state() const noexcept
    {
        return impl_->state;
    }

    bool PCB::setState(ProcessState newState, Tick currentTick)
    {
        if (!isValidTransition(impl_->state, newState))
        {
            return false;
        }

        // Track timing based on state being left
        Tick elapsed = currentTick - impl_->timing.lastStateChange;
        switch (impl_->state)
        {
        case ProcessState::Running:
            impl_->timing.totalCpuTime += elapsed;
            break;
        case ProcessState::Ready:
            impl_->timing.totalWaitTime += elapsed;
            break;
        case ProcessState::Blocked:
            impl_->timing.totalBlockedTime += elapsed;
            break;
        default:
            break;
        }

        // Record first start time
        if (newState == ProcessState::Running && impl_->timing.startTime == 0)
        {
            impl_->timing.startTime = currentTick;
        }

        // Record finish time
        if (newState == ProcessState::Terminated)
        {
            impl_->timing.finishTime = currentTick;
        }

        impl_->state = newState;
        impl_->timing.lastStateChange = currentTick;
        return true;
    }

    const Priority &PCB::priority() const noexcept
    {
        return impl_->priority;
    }

    void PCB::setPriority(const Priority &priority)
    {
        impl_->priority = priority;
    }

    void PCB::setEffectivePriority(PriorityLevel level)
    {
        impl_->priority.effective = level;
    }

    void PCB::setNice(std::int32_t nice)
    {
        impl_->priority.nice = Priority::clampNice(nice);
    }

    const ProcessTiming &PCB::timing() const noexcept
    {
        return impl_->timing;
    }

    ProcessTiming &PCB::timing() noexcept
    {
        return impl_->timing;
    }

    void PCB::addCpuTime(Tick ticks)
    {
        impl_->timing.totalCpuTime += ticks;
    }

    void PCB::addWaitTime(Tick ticks)
    {
        impl_->timing.totalWaitTime += ticks;
    }

    void PCB::addBlockedTime(Tick ticks)
    {
        impl_->timing.totalBlockedTime += ticks;
    }

    const ProcessAddressInfo &PCB::addressInfo() const noexcept
    {
        return impl_->addressInfo;
    }

    ProcessAddressInfo &PCB::addressInfo() noexcept
    {
        return impl_->addressInfo;
    }

} // namespace contur
