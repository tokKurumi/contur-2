/// @file semaphore.cpp
/// @brief Semaphore implementation.

#include "contur/sync/semaphore.h"

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

    struct Semaphore::Impl
    {
        std::size_t count = 0;
        std::size_t maxCount = 1;
        std::deque<ProcessId> waitQueue;
        std::unordered_map<ProcessId, std::size_t> holderCounts;
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

            if (waitQueue.empty())
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

            for (const auto &[holderPid, holdCount] : holderCounts)
            {
                if (holdCount == 0)
                {
                    continue;
                }
                ensurePriorityRegistered(holderPid);
                PriorityLevel current = effectivePriorityByPid[holderPid];
                if (isHigherPriority(highestWaiterPriority.value(), current))
                {
                    effectivePriorityByPid[holderPid] = highestWaiterPriority.value();
                }
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

    Semaphore::Semaphore(std::size_t initialCount, std::size_t maxCount)
        : impl_(std::make_unique<Impl>())
    {
        impl_->maxCount = std::max<std::size_t>(1, maxCount);
        impl_->count = std::min(initialCount, impl_->maxCount);
    }

    Semaphore::~Semaphore() = default;
    Semaphore::Semaphore(Semaphore &&) noexcept = default;
    Semaphore &Semaphore::operator=(Semaphore &&) noexcept = default;

    Result<void> Semaphore::acquire(ProcessId pid)
    {
        if (pid == INVALID_PID)
        {
            return Result<void>::error(ErrorCode::InvalidPid);
        }

        impl_->ensurePriorityRegistered(pid);

        if (impl_->count > 0)
        {
            --impl_->count;
            ++impl_->holderCounts[pid];
            impl_->recomputePriorities();
            return Result<void>::ok();
        }

        if (std::find(impl_->waitQueue.begin(), impl_->waitQueue.end(), pid) == impl_->waitQueue.end())
        {
            impl_->waitQueue.push_back(pid);
        }
        impl_->recomputePriorities();
        return Result<void>::error(ErrorCode::ResourceBusy);
    }

    Result<void> Semaphore::release(ProcessId pid)
    {
        if (pid == INVALID_PID)
        {
            return Result<void>::error(ErrorCode::InvalidPid);
        }

        impl_->ensurePriorityRegistered(pid);
        if (auto holder = impl_->holderCounts.find(pid); holder != impl_->holderCounts.end())
        {
            if (holder->second > 0)
            {
                --holder->second;
            }
            if (holder->second == 0)
            {
                impl_->holderCounts.erase(holder);
            }
        }

        if (!impl_->waitQueue.empty())
        {
            ProcessId next = impl_->waitQueue.front();
            impl_->waitQueue.pop_front();
            impl_->ensurePriorityRegistered(next);
            ++impl_->holderCounts[next];
            impl_->recomputePriorities();
            return Result<void>::ok();
        }

        if (impl_->count >= impl_->maxCount)
        {
            impl_->recomputePriorities();
            return Result<void>::error(ErrorCode::InvalidState);
        }

        ++impl_->count;
        impl_->recomputePriorities();
        return Result<void>::ok();
    }

    Result<void> Semaphore::tryAcquire(ProcessId pid)
    {
        if (pid == INVALID_PID)
        {
            return Result<void>::error(ErrorCode::InvalidPid);
        }

        impl_->ensurePriorityRegistered(pid);

        if (impl_->count == 0)
        {
            return Result<void>::error(ErrorCode::ResourceBusy);
        }

        --impl_->count;
        ++impl_->holderCounts[pid];
        impl_->recomputePriorities();
        return Result<void>::ok();
    }

    std::string_view Semaphore::name() const noexcept
    {
        return "Semaphore";
    }

    SyncLayer Semaphore::layer() const noexcept
    {
        return SyncLayer::SimulatedResource;
    }

    Result<void> Semaphore::registerProcessPriority(ProcessId pid, PriorityLevel basePriority)
    {
        return impl_->registerPriority(pid, basePriority);
    }

    PriorityLevel Semaphore::effectivePriority(ProcessId pid) const noexcept
    {
        return impl_->effectivePriority(pid);
    }

    PriorityLevel Semaphore::basePriority(ProcessId pid) const noexcept
    {
        return impl_->basePriority(pid);
    }

    std::size_t Semaphore::count() const noexcept
    {
        return impl_->count;
    }

    std::size_t Semaphore::maxCount() const noexcept
    {
        return impl_->maxCount;
    }

    std::size_t Semaphore::waitingCount() const noexcept
    {
        return impl_->waitQueue.size();
    }

} // namespace contur
