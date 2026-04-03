/// @file tracer.cpp
/// @brief Active tracer implementation.

#include "contur/tracing/tracer.h"

#include <memory>
#include <mutex>
#include <utility>

#include "contur/core/clock.h"

#include "contur/tracing/trace_sink.h"

namespace contur {

    namespace {

        class NullSink final : public ITraceSink
        {
            public:
            void write(const TraceEvent &event) override
            {
                (void)event;
            }
        };

        [[nodiscard]] std::unique_ptr<ITraceSink> normalizeSink(std::unique_ptr<ITraceSink> sink)
        {
            if (sink)
            {
                return sink;
            }
            return std::make_unique<NullSink>();
        }

    } // namespace

    struct Tracer::Impl
    {
        explicit Impl(std::unique_ptr<ITraceSink> traceSink, const IClock &traceClock)
            : sink(std::move(traceSink))
            , traceClock(traceClock)
        {}

        std::unique_ptr<ITraceSink> sink;
        const IClock &traceClock;
        mutable std::mutex sinkMutex;
        TraceLevel minLevel = TraceLevel::Debug;

        static thread_local std::uint32_t depth;
    };

    thread_local std::uint32_t Tracer::Impl::depth = 0;

    Tracer::Tracer(std::unique_ptr<ITraceSink> sink, const IClock &clock)
        : impl_(std::make_unique<Impl>(normalizeSink(std::move(sink)), clock))
    {}

    Tracer::~Tracer() = default;
    Tracer::Tracer(Tracer &&) noexcept = default;
    Tracer &Tracer::operator=(Tracer &&) noexcept = default;

    void Tracer::trace(const TraceEvent &event)
    {
        if (static_cast<std::uint8_t>(event.level) < static_cast<std::uint8_t>(impl_->minLevel))
        {
            return;
        }

        std::lock_guard<std::mutex> lock(impl_->sinkMutex);
        impl_->sink->write(event);
    }

    void Tracer::pushScope(std::string_view subsystem, std::string_view operation)
    {
        trace(makeTraceEvent(impl_->traceClock.now(), subsystem, operation, "enter", currentDepth()));
        ++Impl::depth;
    }

    void Tracer::popScope()
    {
        if (Impl::depth == 0)
        {
            return;
        }
        --Impl::depth;
    }

    std::uint32_t Tracer::currentDepth() const noexcept
    {
        return Impl::depth;
    }

    void Tracer::setMinLevel(TraceLevel level) noexcept
    {
        impl_->minLevel = level;
    }

    TraceLevel Tracer::minLevel() const noexcept
    {
        return impl_->minLevel;
    }

    const IClock &Tracer::clock() const noexcept
    {
        return impl_->traceClock;
    }

} // namespace contur
