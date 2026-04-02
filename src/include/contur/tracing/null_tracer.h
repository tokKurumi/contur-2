/// @file null_tracer.h
/// @brief Null-object tracer implementation.

#pragma once

#include "contur/tracing/i_tracer.h"

namespace contur {

    class IClock;

    /// @brief No-op tracer used when tracing is disabled.
    class NullTracer final : public ITracer
    {
        public:
        /// @brief Constructs a no-op tracer.
        /// @param clock Clock reference returned by clock().
        explicit NullTracer(const IClock &clock)
            : clock_(clock)
        {}

        /// @brief Destroys no-op tracer.
        ~NullTracer() override = default;

        /// @brief Copy construction is disabled.
        NullTracer(const NullTracer &) = delete;

        /// @brief Copy assignment is disabled.
        NullTracer &operator=(const NullTracer &) = delete;
        /// @brief Move constructor is deleted because of reference member.
        NullTracer(NullTracer &&) = delete;

        /// @brief Move assignment is deleted because of reference member.
        NullTracer &operator=(NullTracer &&) = delete;

        /// @copydoc ITracer::trace
        void trace(const TraceEvent &event) override
        {
            (void)event;
        }

        /// @copydoc ITracer::pushScope
        void pushScope(std::string_view subsystem, std::string_view operation) override
        {
            (void)subsystem;
            (void)operation;
        }

        /// @copydoc ITracer::popScope
        void popScope() override
        {}

        /// @copydoc ITracer::currentDepth
        [[nodiscard]] std::uint32_t currentDepth() const noexcept override
        {
            return 0;
        }

        /// @copydoc ITracer::setMinLevel
        void setMinLevel(TraceLevel level) noexcept override
        {
            (void)level;
        }

        /// @copydoc ITracer::minLevel
        [[nodiscard]] TraceLevel minLevel() const noexcept override
        {
            return TraceLevel::Error;
        }

        /// @copydoc ITracer::clock
        [[nodiscard]] const IClock &clock() const noexcept override
        {
            return clock_;
        }

        private:
        const IClock &clock_;
    };

} // namespace contur
