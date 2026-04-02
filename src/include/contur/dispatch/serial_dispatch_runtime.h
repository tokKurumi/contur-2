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
        ~SerialDispatchRuntime() override;

        SerialDispatchRuntime(const SerialDispatchRuntime &) = delete;
        SerialDispatchRuntime &operator=(const SerialDispatchRuntime &) = delete;
        SerialDispatchRuntime(SerialDispatchRuntime &&) noexcept;
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