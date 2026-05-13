/// @file test_console_sink.cpp
/// @brief Unit tests for ConsoleSink — verifies it consumes events without throwing
///        and respects the ITraceSink contract on all levels and depths.

#include <atomic>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "contur/tracing/console_sink.h"
#include "contur/tracing/trace_event.h"
#include "contur/tracing/trace_sink.h"

using namespace contur;

namespace {

    class CoutRedirect
    {
        public:
        explicit CoutRedirect(std::streambuf *target)
            : oldBuf_(std::cout.rdbuf(target))
        {}
        ~CoutRedirect()
        {
            std::cout.rdbuf(oldBuf_);
        }
        CoutRedirect(const CoutRedirect &) = delete;
        CoutRedirect &operator=(const CoutRedirect &) = delete;

        private:
        std::streambuf *oldBuf_;
    };

} // namespace

TEST(ConsoleSinkTest, WriteSingleEventDoesNotThrow)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleSink sink;
    EXPECT_NO_THROW(sink.write(makeTraceEvent(1, "Kernel", "createProcess", "pid=1", 0)));

    const auto output = captured.str();
    EXPECT_NE(output.find("Kernel"), std::string::npos);
    EXPECT_NE(output.find("createProcess"), std::string::npos);
    EXPECT_NE(output.find("pid=1"), std::string::npos);
}

TEST(ConsoleSinkTest, OutputContainsTimestampAndLevel)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleSink sink;
    sink.write(makeTraceEvent(42, "Scheduler", "tick", "", 0, TraceLevel::Warn));

    const auto output = captured.str();
    EXPECT_NE(output.find("T=42"), std::string::npos);
    EXPECT_NE(output.find("warn"), std::string::npos);
}

TEST(ConsoleSinkTest, OutputIndentsByDepth)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleSink sink;
    sink.write(makeTraceEvent(1, "Sub", "op", "", 3));

    const auto output = captured.str();
    // depth=3 -> 6 spaces of indent before subsystem name
    const auto subsystemPos = output.find("Sub::op");
    ASSERT_NE(subsystemPos, std::string::npos);
    EXPECT_GE(subsystemPos, 6u);
    EXPECT_EQ(output.substr(subsystemPos - 6, 6), std::string("      "));
}

TEST(ConsoleSinkTest, OmitsDetailsArrowWhenEmpty)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleSink sink;
    sink.write(makeTraceEvent(1, "Kernel", "boot", "", 0));

    EXPECT_EQ(captured.str().find(" -> "), std::string::npos);
}

TEST(ConsoleSinkTest, IncludesArrowWhenDetailsPresent)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleSink sink;
    sink.write(makeTraceEvent(1, "Kernel", "boot", "ok", 0));

    EXPECT_NE(captured.str().find(" -> ok"), std::string::npos);
}

TEST(ConsoleSinkTest, AllTraceLevelsRender)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleSink sink;
    sink.write(makeTraceEvent(1, "S", "op", "", 0, TraceLevel::Debug));
    sink.write(makeTraceEvent(2, "S", "op", "", 0, TraceLevel::Info));
    sink.write(makeTraceEvent(3, "S", "op", "", 0, TraceLevel::Warn));
    sink.write(makeTraceEvent(4, "S", "op", "", 0, TraceLevel::Error));

    const auto output = captured.str();
    EXPECT_NE(output.find("debug"), std::string::npos);
    EXPECT_NE(output.find("info"), std::string::npos);
    EXPECT_NE(output.find("warn"), std::string::npos);
    EXPECT_NE(output.find("error"), std::string::npos);
}

TEST(ConsoleSinkTest, MoveConstructionPreservesUsability)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleSink original;
    ConsoleSink moved(std::move(original));
    EXPECT_NO_THROW(moved.write(makeTraceEvent(1, "Kernel", "op", "", 0)));

    EXPECT_NE(captured.str().find("Kernel::op"), std::string::npos);
}

TEST(ConsoleSinkTest, ConcurrentWritesDoNotCrash)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleSink sink;
    constexpr int threadCount = 4;
    constexpr int writesPerThread = 50;

    std::vector<std::thread> threads;
    threads.reserve(threadCount);
    std::atomic<int> writes{0};

    for (int t = 0; t < threadCount; ++t)
    {
        threads.emplace_back([&sink, &writes] {
            for (int i = 0; i < writesPerThread; ++i)
            {
                sink.write(makeTraceEvent(static_cast<Tick>(i), "Concurrent", "tick", "", 0));
                writes.fetch_add(1);
            }
        });
    }
    for (auto &thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(writes.load(), threadCount * writesPerThread);
}

TEST(ConsoleSinkTest, ImplementsITraceSinkInterface)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    std::unique_ptr<ITraceSink> sink = std::make_unique<ConsoleSink>();
    EXPECT_NO_THROW(sink->write(makeTraceEvent(7, "Iface", "op", "", 0)));

    EXPECT_NE(captured.str().find("Iface::op"), std::string::npos);
}
