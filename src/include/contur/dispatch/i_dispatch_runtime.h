/// @file i_dispatch_runtime.h
/// @brief Runtime strategy interface for dispatching across dispatcher lanes.

#pragma once

#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

#include "contur/core/error.h"
#include "contur/dispatch/threading_config.h"

namespace contur {

    class IDispatcher;

    /// @brief Alias for the dispatcher lanes controlled by a runtime.
    using DispatcherLanes = std::vector<std::reference_wrapper<IDispatcher>>;

    /// @brief Strategy interface that executes dispatch/tick across dispatcher lanes.
    class IDispatchRuntime
    {
        public:
        virtual ~IDispatchRuntime() = default;

        /// @brief Returns runtime implementation name.
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;

        /// @brief Returns the normalized runtime-owned threading config.
        [[nodiscard]] virtual const HostThreadingConfig &config() const noexcept = 0;

        /// @brief Runs one dispatch cycle across provided lanes.
        /// @param lanes Dispatcher lanes to execute.
        /// @param tickBudget Tick budget forwarded to each lane dispatch.
        /// @return Ok if any lane made progress, or an aggregated error.
        [[nodiscard]] virtual Result<void> dispatch(const DispatcherLanes &lanes, std::size_t tickBudget) = 0;

        /// @brief Advances all lanes by one runtime tick.
        /// @param lanes Dispatcher lanes to tick.
        virtual void tick(const DispatcherLanes &lanes) = 0;

        protected:
        IDispatchRuntime() = default;
        IDispatchRuntime(const IDispatchRuntime &) = default;
        IDispatchRuntime &operator=(const IDispatchRuntime &) = default;
        IDispatchRuntime(IDispatchRuntime &&) = default;
        IDispatchRuntime &operator=(IDispatchRuntime &&) = default;
    };

} // namespace contur