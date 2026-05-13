/// @file test_file_sink.cpp
/// @brief Unit tests for FileSink — verifies persistence, formatting, append semantics,
///        thread safety, and graceful handling of unwritable paths.

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "contur/tracing/file_sink.h"
#include "contur/tracing/trace_event.h"
#include "contur/tracing/trace_sink.h"

using namespace contur;

namespace {

    struct TempFileGuard
    {
        std::filesystem::path path;

        explicit TempFileGuard(const std::string &suffix)
        {
            const auto stem = "contur2_file_sink_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + "_" + suffix;
            path = std::filesystem::temp_directory_path() / stem;
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }

        ~TempFileGuard()
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }

        TempFileGuard(const TempFileGuard &) = delete;
        TempFileGuard &operator=(const TempFileGuard &) = delete;
    };

    std::string readFile(const std::filesystem::path &path)
    {
        std::ifstream input(path, std::ios::in);
        std::stringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

    std::size_t countLines(const std::string &content)
    {
        return static_cast<std::size_t>(std::count(content.begin(), content.end(), '\n'));
    }

} // namespace

TEST(FileSinkTest, WritesSingleEventToFile)
{
    TempFileGuard guard("single.log");

    {
        FileSink sink(guard.path.string());
        sink.write(makeTraceEvent(11, "Kernel", "boot", "ok", 0));
    }

    const auto content = readFile(guard.path);
    EXPECT_NE(content.find("Kernel::boot"), std::string::npos);
    EXPECT_NE(content.find(" -> ok"), std::string::npos);
    EXPECT_NE(content.find("T=11"), std::string::npos);
}

TEST(FileSinkTest, EventsTerminatedByNewline)
{
    TempFileGuard guard("newline.log");

    {
        FileSink sink(guard.path.string());
        sink.write(makeTraceEvent(1, "A", "op", "", 0));
        sink.write(makeTraceEvent(2, "B", "op", "", 0));
        sink.write(makeTraceEvent(3, "C", "op", "", 0));
    }

    const auto content = readFile(guard.path);
    EXPECT_EQ(countLines(content), 3u);
}

TEST(FileSinkTest, OutputContainsLevelTag)
{
    TempFileGuard guard("level.log");

    {
        FileSink sink(guard.path.string());
        sink.write(makeTraceEvent(1, "S", "op", "", 0, TraceLevel::Error));
    }

    EXPECT_NE(readFile(guard.path).find("error"), std::string::npos);
}

TEST(FileSinkTest, OutputIndentsByDepth)
{
    TempFileGuard guard("indent.log");

    {
        FileSink sink(guard.path.string());
        sink.write(makeTraceEvent(1, "S", "op", "", 2));
    }

    const auto content = readFile(guard.path);
    const auto subsystemPos = content.find("S::op");
    ASSERT_NE(subsystemPos, std::string::npos);
    ASSERT_GE(subsystemPos, 4u);
    EXPECT_EQ(content.substr(subsystemPos - 4, 4), std::string("    "));
}

TEST(FileSinkTest, AppendsAcrossSinkLifecycles)
{
    TempFileGuard guard("append.log");

    {
        FileSink sink(guard.path.string());
        sink.write(makeTraceEvent(1, "First", "op", "", 0));
    }
    {
        FileSink sink(guard.path.string());
        sink.write(makeTraceEvent(2, "Second", "op", "", 0));
    }

    const auto content = readFile(guard.path);
    EXPECT_NE(content.find("First::op"), std::string::npos);
    EXPECT_NE(content.find("Second::op"), std::string::npos);
    EXPECT_EQ(countLines(content), 2u);
}

TEST(FileSinkTest, OmitsDetailsArrowWhenDetailsEmpty)
{
    TempFileGuard guard("no-details.log");

    {
        FileSink sink(guard.path.string());
        sink.write(makeTraceEvent(1, "K", "op", "", 0));
    }

    EXPECT_EQ(readFile(guard.path).find(" -> "), std::string::npos);
}

TEST(FileSinkTest, UnopenableFileSilentlyDropsWrites)
{
    // Path inside a non-existent directory will fail to open;
    // sink must not throw on construction or on write.
    const std::filesystem::path bogus = std::filesystem::temp_directory_path() / "contur2_no_such_dir_xyzzy" / "out.log";

    std::unique_ptr<FileSink> sink;
    EXPECT_NO_THROW(sink = std::make_unique<FileSink>(bogus.string()));
    EXPECT_NO_THROW(sink->write(makeTraceEvent(1, "X", "op", "", 0)));

    // File should not have been created — its parent didn't exist.
    EXPECT_FALSE(std::filesystem::exists(bogus));
}

TEST(FileSinkTest, ConcurrentWritesPreserveLineIntegrity)
{
    TempFileGuard guard("concurrent.log");

    constexpr int threadCount = 4;
    constexpr int writesPerThread = 100;

    {
        FileSink sink(guard.path.string());

        std::vector<std::thread> threads;
        threads.reserve(threadCount);
        std::atomic<int> totalWrites{0};

        for (int t = 0; t < threadCount; ++t)
        {
            threads.emplace_back([&sink, &totalWrites] {
                for (int i = 0; i < writesPerThread; ++i)
                {
                    sink.write(makeTraceEvent(static_cast<Tick>(i), "Concurrent", "tick", "marker", 0));
                    totalWrites.fetch_add(1);
                }
            });
        }
        for (auto &thread : threads)
        {
            thread.join();
        }

        EXPECT_EQ(totalWrites.load(), threadCount * writesPerThread);
    }

    const auto content = readFile(guard.path);
    EXPECT_EQ(countLines(content), static_cast<std::size_t>(threadCount * writesPerThread));

    // Each line must contain "Concurrent::tick" — no torn output (mutex must serialize writes).
    std::istringstream iss(content);
    std::string line;
    std::size_t goodLines = 0;
    while (std::getline(iss, line))
    {
        if (line.find("Concurrent::tick") != std::string::npos)
        {
            ++goodLines;
        }
    }
    EXPECT_EQ(goodLines, static_cast<std::size_t>(threadCount * writesPerThread));
}

TEST(FileSinkTest, ImplementsITraceSinkInterface)
{
    TempFileGuard guard("iface.log");

    {
        std::unique_ptr<ITraceSink> sink = std::make_unique<FileSink>(guard.path.string());
        sink->write(makeTraceEvent(1, "I", "op", "ok", 0));
    }

    EXPECT_NE(readFile(guard.path).find("I::op"), std::string::npos);
}
