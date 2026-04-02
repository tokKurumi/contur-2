/// @file tracer.h
/// @brief Active tracer implementation writing events to a sink.

#pragma once

#include <memory>

#include "contur/tracing/i_tracer.h"

namespace contur {

    class IClock;
    class ITraceSink;

    /// @brief Active tracer implementation.
    ///
    /// Tracer emits structured events to an injected sink and maintains
    /// hierarchical scope depth per calling thread.
    class Tracer final : public ITracer
    {
        public:
        /// @brief Constructs an active tracer.
        /// @param sink Trace sink receiving emitted events.
        /// @param clock Simulation clock used for timestamping.
        explicit Tracer(std::unique_ptr<ITraceSink> sink, const IClock &clock);

        /// @brief Destroys tracer and owned sink.
        ~Tracer() override;

        /// @brief Copy construction is disabled.
        Tracer(const Tracer &) = delete;

        /// @brief Copy assignment is disabled.
        Tracer &operator=(const Tracer &) = delete;
        /// @brief Move-constructs tracer state.
        Tracer(Tracer &&) noexcept;

        /// @brief Move-assigns tracer state.
        Tracer &operator=(Tracer &&) noexcept;

        /// @copydoc ITracer::trace
        void trace(const TraceEvent &event) override;

        /// @copydoc ITracer::pushScope
        void pushScope(std::string_view subsystem, std::string_view operation) override;

        /// @copydoc ITracer::popScope
        void popScope() override;

        /// @copydoc ITracer::currentDepth
        [[nodiscard]] std::uint32_t currentDepth() const noexcept override;

        /// @copydoc ITracer::setMinLevel
        void setMinLevel(TraceLevel level) noexcept override;

        /// @copydoc ITracer::minLevel
        [[nodiscard]] TraceLevel minLevel() const noexcept override;

        /// @copydoc ITracer::clock
        [[nodiscard]] const IClock &clock() const noexcept override;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
