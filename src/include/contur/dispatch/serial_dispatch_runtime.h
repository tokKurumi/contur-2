/// @file serial_dispatch_runtime.h
/// @brief Serial dispatch runtime implementation.

#pragma once

#include <memory>

#include "contur/dispatch/i_dispatch_runtime.h"

namespace contur {

    /// @brief Serial runtime strategy that executes dispatcher lanes on the caller thread.
    class SerialDispatchRuntime final : public IDispatchRuntime
    {
        public:
        /// @brief Constructs serial runtime with runtime-owned config.
        /// @param config Runtime threading config (normalized to serial-safe invariants).
        explicit SerialDispatchRuntime(HostThreadingConfig config = {});

        /// @brief Destroys serial runtime.
        ~SerialDispatchRuntime() override;

        /// @brief Copy construction is disabled.
        SerialDispatchRuntime(const SerialDispatchRuntime &) = delete;

        /// @brief Copy assignment is disabled.
        SerialDispatchRuntime &operator=(const SerialDispatchRuntime &) = delete;
        /// @brief Move-constructs serial runtime state.
        SerialDispatchRuntime(SerialDispatchRuntime &&) noexcept;

        /// @brief Move-assigns serial runtime state.
        SerialDispatchRuntime &operator=(SerialDispatchRuntime &&) noexcept;

        /// @brief Returns runtime implementation name.
        [[nodiscard]] std::string_view name() const noexcept override;

        /// @brief Returns runtime-owned threading config.
        [[nodiscard]] const HostThreadingConfig &config() const noexcept override;

        /// @brief Runs one serial dispatch pass across all lanes.
        [[nodiscard]] Result<void> dispatch(const DispatcherLanes &lanes, std::size_t tickBudget) override;

        /// @brief Ticks all lanes serially.
        void tick(const DispatcherLanes &lanes) override;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur