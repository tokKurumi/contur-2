/// @file test_pipe_extended.cpp
/// @brief Extended unit tests for Pipe — partial reads, interleaved ops,
///        FIFO ordering across multiple writes, size tracking, move semantics.

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "contur/core/error.h"

#include "contur/ipc/pipe.h"

using namespace contur;

namespace {

    std::vector<std::byte> bytes(std::initializer_list<unsigned char> vals)
    {
        std::vector<std::byte> out;
        for (auto v : vals)
        {
            out.push_back(static_cast<std::byte>(v));
        }
        return out;
    }

} // namespace

// Size tracking across operations
TEST(PipeExtTest, SizeTracksDataInBuffer)
{
    Pipe pipe("p", 16);
    EXPECT_EQ(pipe.size(), 0u);

    ASSERT_TRUE(pipe.write(bytes({1, 2, 3})).isOk());
    EXPECT_EQ(pipe.size(), 3u);

    std::vector<std::byte> buf(2);
    ASSERT_TRUE(pipe.read(buf).isOk());
    EXPECT_EQ(pipe.size(), 1u);

    ASSERT_TRUE(pipe.write(bytes({4, 5})).isOk());
    EXPECT_EQ(pipe.size(), 3u);
}

// FIFO ordering across multiple writes
TEST(PipeExtTest, MultiplleWritesReadBackInFifoOrder)
{
    Pipe pipe("fifo", 32);

    ASSERT_TRUE(pipe.write(bytes({10, 11})).isOk());
    ASSERT_TRUE(pipe.write(bytes({20, 21})).isOk());
    ASSERT_TRUE(pipe.write(bytes({30})).isOk());

    std::vector<std::byte> buf(5);
    auto r = pipe.read(buf);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 5u);
    EXPECT_EQ(buf[0], std::byte{10});
    EXPECT_EQ(buf[1], std::byte{11});
    EXPECT_EQ(buf[2], std::byte{20});
    EXPECT_EQ(buf[3], std::byte{21});
    EXPECT_EQ(buf[4], std::byte{30});
}

// Partial read (buffer larger than available data)
TEST(PipeExtTest, ReadRequestingMoreThanAvailableReturnsWhatIsThere)
{
    Pipe pipe("partial", 8);
    ASSERT_TRUE(pipe.write(bytes({1, 2})).isOk());

    std::vector<std::byte> buf(8, std::byte{0xFF});
    auto r = pipe.read(buf);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 2u);
    EXPECT_EQ(buf[0], std::byte{1});
    EXPECT_EQ(buf[1], std::byte{2});
    // Bytes beyond what was read should be untouched
    EXPECT_EQ(buf[2], std::byte{0xFF});

    EXPECT_EQ(pipe.size(), 0u);
}

// Interleaved writes and reads
TEST(PipeExtTest, InterleavedWritesAndReadsPreserveOrder)
{
    Pipe pipe("interleave", 16);

    ASSERT_TRUE(pipe.write(bytes({1})).isOk());
    std::vector<std::byte> b1(1);
    ASSERT_TRUE(pipe.read(b1).isOk());
    EXPECT_EQ(b1[0], std::byte{1});

    ASSERT_TRUE(pipe.write(bytes({2, 3})).isOk());
    std::vector<std::byte> b2(1);
    ASSERT_TRUE(pipe.read(b2).isOk());
    EXPECT_EQ(b2[0], std::byte{2});

    ASSERT_TRUE(pipe.write(bytes({4})).isOk());
    std::vector<std::byte> b3(2);
    auto r = pipe.read(b3);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 2u);
    EXPECT_EQ(b3[0], std::byte{3});
    EXPECT_EQ(b3[1], std::byte{4});
}

// Capacity = 1 boundary
TEST(PipeExtTest, CapacityOnePipeAcceptsOneByte)
{
    Pipe pipe("one", 1);

    auto w = pipe.write(bytes({0xAB}));
    ASSERT_TRUE(w.isOk());
    EXPECT_EQ(w.value(), 1u);
    EXPECT_EQ(pipe.size(), 1u);

    // Second write must fail
    auto w2 = pipe.write(bytes({0xCD}));
    ASSERT_TRUE(w2.isError());
    EXPECT_EQ(w2.errorCode(), ErrorCode::BufferFull);

    std::vector<std::byte> out(1);
    ASSERT_TRUE(pipe.read(out).isOk());
    EXPECT_EQ(out[0], std::byte{0xAB});
    EXPECT_EQ(pipe.size(), 0u);
}

// Close clears buffered data
TEST(PipeExtTest, CloseAfterWriteMakesReadFail)
{
    Pipe pipe("closedpipe", 8);
    ASSERT_TRUE(pipe.write(bytes({1, 2})).isOk());
    EXPECT_EQ(pipe.size(), 2u);

    pipe.close();
    EXPECT_FALSE(pipe.isOpen());

    std::vector<std::byte> buf(2);
    auto r = pipe.read(buf);
    ASSERT_TRUE(r.isError());
    EXPECT_EQ(r.errorCode(), ErrorCode::InvalidState);
}

TEST(PipeExtTest, DoubleCloseIsHarmless)
{
    Pipe pipe("dc", 4);
    pipe.close();
    pipe.close(); // must not crash
    EXPECT_FALSE(pipe.isOpen());
}

// Move semantics
TEST(PipeExtTest, MoveConstructedPipeOwnsState)
{
    Pipe original("orig", 8);
    ASSERT_TRUE(original.write(bytes({7, 8})).isOk());

    Pipe moved(std::move(original));

    EXPECT_TRUE(moved.isOpen());
    EXPECT_EQ(moved.name(), "orig");
    EXPECT_EQ(moved.size(), 2u);

    std::vector<std::byte> buf(2);
    ASSERT_TRUE(moved.read(buf).isOk());
    EXPECT_EQ(buf[0], std::byte{7});
    EXPECT_EQ(buf[1], std::byte{8});
}

TEST(PipeExtTest, MoveAssignedPipeOwnsState)
{
    Pipe src("src", 4);
    ASSERT_TRUE(src.write(bytes({5})).isOk());

    Pipe dst("dst", 2);
    dst = std::move(src);

    EXPECT_EQ(dst.name(), "src");
    EXPECT_EQ(dst.size(), 1u);
}
