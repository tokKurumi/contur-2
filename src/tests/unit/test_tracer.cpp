/// @file test_tracer.cpp
/// @brief Unit tests for Tracer, NullTracer and TraceScope.

#include <memory>

#include <gtest/gtest.h>

#include "contur/core/clock.h"
#include "contur/tracing/buffer_sink.h"
#include "contur/tracing/null_tracer.h"
#include "contur/tracing/trace_scope.h"
#include "contur/tracing/tracer.h"

using namespace contur;

TEST(TracerTest, NestedScopesEmitDepthSequence)
{
    SimulationClock clock;

    auto sink = std::make_unique<BufferSink>();
    BufferSink *sinkRaw = sink.get();
    Tracer tracer(std::move(sink), clock);

    {
        TraceScope level0(tracer, "Kernel", "createProcess");
        {
            TraceScope level1(tracer, "Dispatcher", "dispatch");
            {
                TraceScope level2(tracer, "CPU", "execute");
                (void)level2;
            }
            (void)level1;
        }
        (void)level0;
    }

    auto events = sinkRaw->snapshot();

    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].depth, 0u);
    EXPECT_EQ(events[1].depth, 1u);
    EXPECT_EQ(events[2].depth, 2u);
    EXPECT_EQ(events[0].operation, "createProcess");
    EXPECT_EQ(events[1].operation, "dispatch");
    EXPECT_EQ(events[2].operation, "execute");
}

TEST(TracerTest, TraceMacroUsesCurrentDepth)
{
    SimulationClock clock;

    auto sink = std::make_unique<BufferSink>();
    BufferSink *sinkRaw = sink.get();
    Tracer tracer(std::move(sink), clock);

    {
        CONTUR_TRACE_SCOPE(tracer, "Kernel", "tick");
        CONTUR_TRACE(tracer, "Kernel", "dispatch", "budget=4");
    }

    auto events = sinkRaw->snapshot();

#ifdef CONTUR_TRACE_ENABLED
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].depth, 0u);
    EXPECT_EQ(events[0].operation, "tick");
    EXPECT_EQ(events[1].depth, 1u);
    EXPECT_EQ(events[1].operation, "dispatch");
    EXPECT_EQ(events[1].details, "budget=4");
#else
    EXPECT_TRUE(events.empty());
#endif
}

TEST(TracerTest, NullTracerRemainsNoOp)
{
    SimulationClock clock;
    NullTracer tracer(clock);

    EXPECT_EQ(tracer.currentDepth(), 0u);

    tracer.pushScope("Kernel", "tick");
    tracer.trace(makeTraceEvent(clock.now(), "Kernel", "tick", "ignored", tracer.currentDepth()));
    tracer.popScope();

    EXPECT_EQ(tracer.currentDepth(), 0u);
    EXPECT_EQ(tracer.clock().now(), clock.now());
}

TEST(TracerTest, MinLevelFiltersLowerSeverityEvents)
{
    SimulationClock clock;

    auto sink = std::make_unique<BufferSink>();
    BufferSink *sinkRaw = sink.get();
    Tracer tracer(std::move(sink), clock);
    tracer.setMinLevel(TraceLevel::Warn);

    tracer.trace(makeTraceEvent(clock.now(), "Kernel", "debug-event", "", 0, TraceLevel::Debug));
    tracer.trace(makeTraceEvent(clock.now(), "Kernel", "warn-event", "", 0, TraceLevel::Warn));
    tracer.trace(makeTraceEvent(clock.now(), "Kernel", "error-event", "", 0, TraceLevel::Error));

    auto events = sinkRaw->snapshot();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].operation, "warn-event");
    EXPECT_EQ(events[1].operation, "error-event");
}
