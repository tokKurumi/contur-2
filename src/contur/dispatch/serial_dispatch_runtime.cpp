/// @file serial_dispatch_runtime.cpp
/// @brief Serial dispatch runtime implementation.

#include "contur/dispatch/serial_dispatch_runtime.h"

#include "contur/dispatch/i_dispatcher.h"

namespace contur {

    struct SerialDispatchRuntime::Impl
    {
        HostThreadingConfig config;

        explicit Impl(HostThreadingConfig runtimeConfig)
            : config(runtimeConfig.normalized())
        {
            // Serial runtime always enforces single-lane host execution.
            config.hostThreadCount = 1U;
            config.workStealingEnabled = false;
        }
    };

    SerialDispatchRuntime::SerialDispatchRuntime(HostThreadingConfig config)
        : impl_(std::make_unique<Impl>(config))
    {}

    SerialDispatchRuntime::~SerialDispatchRuntime() = default;
    SerialDispatchRuntime::SerialDispatchRuntime(SerialDispatchRuntime &&) noexcept = default;
    SerialDispatchRuntime &SerialDispatchRuntime::operator=(SerialDispatchRuntime &&) noexcept = default;

    std::string_view SerialDispatchRuntime::name() const noexcept
    {
        return "SerialDispatchRuntime";
    }

    const HostThreadingConfig &SerialDispatchRuntime::config() const noexcept
    {
        return impl_->config;
    }

    Result<void> SerialDispatchRuntime::dispatch(const DispatcherLanes &lanes, std::size_t tickBudget)
    {
        if (lanes.empty())
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        bool anySuccess = false;
        ErrorCode firstError = ErrorCode::Ok;

        for (IDispatcher &dispatcher : lanes)
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

    void SerialDispatchRuntime::tick(const DispatcherLanes &lanes)
    {
        for (IDispatcher &dispatcher : lanes)
        {
            dispatcher.tick();
        }
    }

} // namespace contur