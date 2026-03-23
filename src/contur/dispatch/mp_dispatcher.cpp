/// @file mp_dispatcher.cpp
/// @brief MPDispatcher implementation.

#include "contur/dispatch/mp_dispatcher.h"

#include <stdexcept>

#include "contur/process/process_image.h"

namespace contur {

    struct MPDispatcher::Impl
    {
        std::vector<std::reference_wrapper<IDispatcher>> dispatchers;
    };

    MPDispatcher::MPDispatcher(std::vector<std::reference_wrapper<IDispatcher>> dispatchers)
        : impl_(std::make_unique<Impl>())
    {
        if (dispatchers.empty())
        {
            throw std::invalid_argument("MPDispatcher requires at least one dispatcher");
        }
        impl_->dispatchers = std::move(dispatchers);
    }

    MPDispatcher::~MPDispatcher() = default;
    MPDispatcher::MPDispatcher(MPDispatcher &&) noexcept = default;
    MPDispatcher &MPDispatcher::operator=(MPDispatcher &&) noexcept = default;

    Result<void> MPDispatcher::createProcess(std::unique_ptr<ProcessImage> process, Tick currentTick)
    {
        if (!process)
        {
            return Result<void>::error(ErrorCode::InvalidArgument);
        }

        ProcessId pid = process->id();
        std::size_t target = static_cast<std::size_t>(pid) % impl_->dispatchers.size();
        return impl_->dispatchers[target].get().createProcess(std::move(process), currentTick);
    }

    Result<void> MPDispatcher::dispatch(std::size_t tickBudget)
    {
        bool anySuccess = false;
        ErrorCode firstError = ErrorCode::Ok;

        for (IDispatcher &dispatcher : impl_->dispatchers)
        {
            Result<void> result = dispatcher.dispatch(tickBudget);
            if (result.isOk())
            {
                anySuccess = true;
                continue;
            }

            if (firstError == ErrorCode::Ok && result.errorCode() != ErrorCode::NotFound)
            {
                firstError = result.errorCode();
            }
        }

        if (anySuccess)
        {
            return Result<void>::ok();
        }
        if (firstError != ErrorCode::Ok)
        {
            return Result<void>::error(firstError);
        }
        return Result<void>::error(ErrorCode::NotFound);
    }

    Result<void> MPDispatcher::terminateProcess(ProcessId pid, Tick currentTick)
    {
        for (IDispatcher &dispatcher : impl_->dispatchers)
        {
            if (dispatcher.hasProcess(pid))
            {
                return dispatcher.terminateProcess(pid, currentTick);
            }
        }

        return Result<void>::error(ErrorCode::NotFound);
    }

    void MPDispatcher::tick()
    {
        for (IDispatcher &dispatcher : impl_->dispatchers)
        {
            dispatcher.tick();
        }
    }

    std::size_t MPDispatcher::processCount() const noexcept
    {
        std::size_t total = 0;
        for (const IDispatcher &dispatcher : impl_->dispatchers)
        {
            total += dispatcher.processCount();
        }
        return total;
    }

    bool MPDispatcher::hasProcess(ProcessId pid) const noexcept
    {
        for (const IDispatcher &dispatcher : impl_->dispatchers)
        {
            if (dispatcher.hasProcess(pid))
            {
                return true;
            }
        }
        return false;
    }

} // namespace contur
