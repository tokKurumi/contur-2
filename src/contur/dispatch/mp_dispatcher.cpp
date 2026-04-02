/// @file mp_dispatcher.cpp
/// @brief MPDispatcher implementation.

#include "contur/dispatch/mp_dispatcher.h"

#include "contur/dispatch/i_dispatch_runtime.h"
#include "contur/process/process_image.h"

namespace contur {

    struct MPDispatcher::Impl
    {
        std::vector<std::reference_wrapper<IDispatcher>> dispatchers;
        std::reference_wrapper<IDispatchRuntime> runtime;

        Impl(std::vector<std::reference_wrapper<IDispatcher>> dispatchers, IDispatchRuntime &runtime)
            : dispatchers(std::move(dispatchers))
            , runtime(runtime)
        {}
    };

    MPDispatcher::MPDispatcher(std::vector<std::reference_wrapper<IDispatcher>> dispatchers, IDispatchRuntime &runtime)
        : impl_(std::make_unique<Impl>(std::move(dispatchers), runtime))
    {}

    MPDispatcher::~MPDispatcher() = default;
    MPDispatcher::MPDispatcher(MPDispatcher &&) noexcept = default;
    MPDispatcher &MPDispatcher::operator=(MPDispatcher &&) noexcept = default;

    Result<void> MPDispatcher::createProcess(std::unique_ptr<ProcessImage> process, Tick currentTick)
    {
        if (!process)
        {
            return Result<void>::error(ErrorCode::InvalidArgument);
        }
        if (impl_->dispatchers.empty())
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        ProcessId pid = process->id();
        std::size_t target = static_cast<std::size_t>(pid) % impl_->dispatchers.size();
        return impl_->dispatchers[target].get().createProcess(std::move(process), currentTick);
    }

    Result<void> MPDispatcher::dispatch(std::size_t tickBudget)
    {
        return impl_->runtime.get().dispatch(impl_->dispatchers, tickBudget);
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
        impl_->runtime.get().tick(impl_->dispatchers);
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
