/// @file dispatcher_pool.h
/// @brief Worker-pool dispatch runtime implementation.

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "contur/dispatch/i_dispatch_runtime.h"
#include "contur/tracing/trace_event.h"

namespace contur {

    /// @brief Host-thread worker pool runtime for dispatcher lanes.
    ///
    /// DispatcherPool owns worker threads and executes lane dispatch/tick jobs
    /// using runtime-owned HostThreadingConfig. Deterministic mode uses stable
    /// static lane partitioning; non-deterministic mode can use dynamic work
    /// stealing when enabled.
    class DispatcherPool final : public IDispatchRuntime
    {
        public:
        /// @brief Constructs a worker pool runtime.
        /// @param config Runtime threading config owned and normalized by the runtime.
        explicit DispatcherPool(HostThreadingConfig config = {});

        /// @brief Destroys pool runtime and joins worker threads.
        ~DispatcherPool() override;

        /// @brief Copy construction is disabled.
        DispatcherPool(const DispatcherPool &) = delete;

        /// @brief Copy assignment is disabled.
        DispatcherPool &operator=(const DispatcherPool &) = delete;
        /// @brief Move-constructs pool runtime state.
        DispatcherPool(DispatcherPool &&) noexcept;

        /// @brief Move-assigns pool runtime state.
        DispatcherPool &operator=(DispatcherPool &&) noexcept;

        /// @brief Returns runtime implementation name.
        [[nodiscard]] std::string_view name() const noexcept override;

        /// @brief Returns runtime-owned normalized threading config.
        [[nodiscard]] const HostThreadingConfig &config() const noexcept override;

        /// @brief Runs one pooled dispatch pass across all lanes.
        [[nodiscard]] Result<void> dispatch(const DispatcherLanes &lanes, std::size_t tickBudget) override;

        /// @brief Ticks lanes using pooled workers.
        void tick(const DispatcherLanes &lanes) override;

        /// @brief Last completed runtime epoch identifier.
        [[nodiscard]] std::uint64_t lastEpoch() const noexcept;

        /// @brief Stable lane order used by deterministic dispatch mode.
        [[nodiscard]] std::vector<std::size_t> lastDeterministicOrder() const;

        /// @brief Runtime trace metadata emitted by the last dispatch/tick pass.
        [[nodiscard]] std::vector<TraceEvent> lastTraceEvents() const;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur