/// @file mutex.cpp
/// @brief Mutex implementation.

#include "contur/sync/mutex.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>

namespace contur {

    namespace {

        [[nodiscard]] bool isHigherPriority(contur::PriorityLevel lhs, contur::PriorityLevel rhs) noexcept
        {
            return static_cast<std::int8_t>(lhs) < static_cast<std::int8_t>(rhs);
        }

    } // namespace

    struct Mutex::Impl
    {
        std::optional<ProcessId> owner;
        std::size_t recursionDepth = 0;
        std::deque<ProcessId> waitQueue;
        std::unordered_map<ProcessId, PriorityLevel> basePriorityByPid;
        std::unordered_map<ProcessId, PriorityLevel> effectivePriorityByPid;

        void ensurePriorityRegistered(ProcessId pid)
        {
            if (pid == INVALID_PID)
            {
                return;
            }

            if (basePriorityByPid.find(pid) == basePriorityByPid.end())
            {
                basePriorityByPid.emplace(pid, PriorityLevel::Normal);
            }

            if (effectivePriorityByPid.find(pid) == effectivePriorityByPid.end())
            {
                effectivePriorityByPid.emplace(pid, basePriorityByPid[pid]);
            }
        }

        void recomputePriorities()
        {
            for (const auto &[pid, base] : basePriorityByPid)
            {
                effectivePriorityByPid[pid] = base;
            }

            if (!owner.has_value() || waitQueue.empty())
            {
                return;
            }

            std::optional<PriorityLevel> highestWaiterPriority;
            for (ProcessId waiter : waitQueue)
            {
                ensurePriorityRegistered(waiter);
                PriorityLevel waiterPriority = effectivePriorityByPid[waiter];
                if (!highestWaiterPriority.has_value() ||
                    isHigherPriority(waiterPriority, highestWaiterPriority.value()))
                {
                    highestWaiterPriority = waiterPriority;
                }
            }

            if (!highestWaiterPriority.has_value())
            {
                return;
            }

            ProcessId ownerPid = owner.value();
            ensurePriorityRegistered(ownerPid);
            PriorityLevel ownerPriority = effectivePriorityByPid[ownerPid];
            if (isHigherPriority(highestWaiterPriority.value(), ownerPriority))
            {
                effectivePriorityByPid[ownerPid] = highestWaiterPriority.value();
            }
        }

        [[nodiscard]] Result<void> registerPriority(ProcessId pid, PriorityLevel basePriority)
        {
            if (pid == INVALID_PID)
            {
                return Result<void>::error(ErrorCode::InvalidPid);
            }

            basePriorityByPid[pid] = basePriority;
            effectivePriorityByPid[pid] = basePriority;
            recomputePriorities();
            return Result<void>::ok();
        }

        [[nodiscard]] PriorityLevel effectivePriority(ProcessId pid) const noexcept
        {
            if (auto it = effectivePriorityByPid.find(pid); it != effectivePriorityByPid.end())
            {
                return it->second;
            }
            if (auto it = basePriorityByPid.find(pid); it != basePriorityByPid.end())
            {
                return it->second;
            }
            return PriorityLevel::Normal;
        }

        [[nodiscard]] PriorityLevel basePriority(ProcessId pid) const noexcept
        {
            if (auto it = basePriorityByPid.find(pid); it != basePriorityByPid.end())
            {
                return it->second;
            }
            return PriorityLevel::Normal;
        }
    };

    Mutex::Mutex()
        : impl_(std::make_unique<Impl>())
    {}

    Mutex::~Mutex() = default;
    Mutex::Mutex(Mutex &&) noexcept = default;
    Mutex &Mutex::operator=(Mutex &&) noexcept = default;

    Result<void> Mutex::acquire(ProcessId pid)
    {
        if (pid == INVALID_PID)
        {
            return Result<void>::error(ErrorCode::InvalidPid);
        }

        impl_->ensurePriorityRegistered(pid);

        if (!impl_->owner.has_value())
        {
            impl_->owner = pid;
            impl_->recursionDepth = 1;
            impl_->recomputePriorities();
            return Result<void>::ok();
        }

        if (impl_->owner.value() == pid)
        {
            ++impl_->recursionDepth;
            return Result<void>::ok();
        }

        if (std::find(impl_->waitQueue.begin(), impl_->waitQueue.end(), pid) == impl_->waitQueue.end())
        {
            impl_->waitQueue.push_back(pid);
        }
        impl_->recomputePriorities();
        return Result<void>::error(ErrorCode::ResourceBusy);
    }

    Result<void> Mutex::release(ProcessId pid)
    {
        if (!impl_->owner.has_value())
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }
        if (impl_->owner.value() != pid)
        {
            return Result<void>::error(ErrorCode::PermissionDenied);
        }

        if (impl_->recursionDepth > 1)
        {
            --impl_->recursionDepth;
            impl_->recomputePriorities();
            return Result<void>::ok();
        }

        impl_->owner.reset();
        impl_->recursionDepth = 0;

        if (!impl_->waitQueue.empty())
        {
            ProcessId next = impl_->waitQueue.front();
            impl_->waitQueue.pop_front();
            impl_->owner = next;
            impl_->recursionDepth = 1;
        }

        impl_->recomputePriorities();

        return Result<void>::ok();
    }

    Result<void> Mutex::tryAcquire(ProcessId pid)
    {
        if (pid == INVALID_PID)
        {
            return Result<void>::error(ErrorCode::InvalidPid);
        }

        impl_->ensurePriorityRegistered(pid);

        if (!impl_->owner.has_value())
        {
            impl_->owner = pid;
            impl_->recursionDepth = 1;
            impl_->recomputePriorities();
            return Result<void>::ok();
        }

        if (impl_->owner.value() == pid)
        {
            ++impl_->recursionDepth;
            impl_->recomputePriorities();
            return Result<void>::ok();
        }

        return Result<void>::error(ErrorCode::ResourceBusy);
    }

    std::string_view Mutex::name() const noexcept
    {
        return "Mutex";
    }

    SyncLayer Mutex::layer() const noexcept
    {
        return SyncLayer::SimulatedResource;
    }

    Result<void> Mutex::registerProcessPriority(ProcessId pid, PriorityLevel basePriority)
    {
        return impl_->registerPriority(pid, basePriority);
    }

    PriorityLevel Mutex::effectivePriority(ProcessId pid) const noexcept
    {
        return impl_->effectivePriority(pid);
    }

    PriorityLevel Mutex::basePriority(ProcessId pid) const noexcept
    {
        return impl_->basePriority(pid);
    }

    bool Mutex::isLocked() const noexcept
    {
        return impl_->owner.has_value();
    }

    std::optional<ProcessId> Mutex::owner() const noexcept
    {
        return impl_->owner;
    }

    std::size_t Mutex::recursionDepth() const noexcept
    {
        return impl_->recursionDepth;
    }

    std::size_t Mutex::waitingCount() const noexcept
    {
        return impl_->waitQueue.size();
    }

} // namespace contur
