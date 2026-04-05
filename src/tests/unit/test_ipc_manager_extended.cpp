/// @file test_ipc_manager_extended.cpp
/// @brief Extended unit tests for IpcManager — mixed channel types, write/read
///        via manager, destroy/recreate, zero-size channels, channel count.

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "contur/core/error.h"

#include "contur/ipc/ipc_manager.h"

using namespace contur;

// Channel count
TEST(IpcManagerExtTest, ChannelCountTracksAllChannelTypes)
{
    IpcManager mgr;
    EXPECT_EQ(mgr.channelCount(), 0u);

    ASSERT_TRUE(mgr.createPipe("p", 16).isOk());
    EXPECT_EQ(mgr.channelCount(), 1u);

    ASSERT_TRUE(mgr.createSharedMemory("s", 32).isOk());
    EXPECT_EQ(mgr.channelCount(), 2u);

    ASSERT_TRUE(mgr.createMessageQueue("q", 4, false).isOk());
    EXPECT_EQ(mgr.channelCount(), 3u);
}

TEST(IpcManagerExtTest, DestroyAllChannelsReducesCountToZero)
{
    IpcManager mgr;
    ASSERT_TRUE(mgr.createPipe("p1", 8).isOk());
    ASSERT_TRUE(mgr.createPipe("p2", 8).isOk());
    ASSERT_TRUE(mgr.createPipe("p3", 8).isOk());
    EXPECT_EQ(mgr.channelCount(), 3u);

    ASSERT_TRUE(mgr.destroyChannel("p1").isOk());
    ASSERT_TRUE(mgr.destroyChannel("p2").isOk());
    ASSERT_TRUE(mgr.destroyChannel("p3").isOk());
    EXPECT_EQ(mgr.channelCount(), 0u);
}

// Write and read via manager lookup
TEST(IpcManagerExtTest, PipeWriteReadViaGetChannel)
{
    IpcManager mgr;
    ASSERT_TRUE(mgr.createPipe("io-pipe", 32).isOk());

    auto chanW = mgr.getChannel("io-pipe");
    ASSERT_TRUE(chanW.isOk());

    const std::vector<std::byte> payload = {std::byte{0xDE}, std::byte{0xAD}};
    auto wrote = chanW.value().get().write(payload);
    ASSERT_TRUE(wrote.isOk());
    EXPECT_EQ(wrote.value(), 2u);

    auto chanR = mgr.getChannel("io-pipe");
    ASSERT_TRUE(chanR.isOk());

    std::vector<std::byte> buf(2);
    auto read = chanR.value().get().read(buf);
    ASSERT_TRUE(read.isOk());
    EXPECT_EQ(read.value(), 2u);
    EXPECT_EQ(buf[0], std::byte{0xDE});
    EXPECT_EQ(buf[1], std::byte{0xAD});
}

// Exists predicate
TEST(IpcManagerExtTest, ExistsReturnsFalseForUnknownName)
{
    IpcManager mgr;
    EXPECT_FALSE(mgr.exists("no-such"));
}

TEST(IpcManagerExtTest, ExistsReturnsTrueAfterCreateAndFalseAfterDestroy)
{
    IpcManager mgr;
    ASSERT_TRUE(mgr.createPipe("ch", 8).isOk());
    EXPECT_TRUE(mgr.exists("ch"));

    ASSERT_TRUE(mgr.destroyChannel("ch").isOk());
    EXPECT_FALSE(mgr.exists("ch"));
}

// Destroy then recreate
TEST(IpcManagerExtTest, DestroyAndRecreateSameNameSucceeds)
{
    IpcManager mgr;
    ASSERT_TRUE(mgr.createPipe("reuse", 8).isOk());
    ASSERT_TRUE(mgr.destroyChannel("reuse").isOk());

    // Different capacity on recreate — must succeed
    ASSERT_TRUE(mgr.createPipe("reuse", 64).isOk());
    EXPECT_TRUE(mgr.exists("reuse"));
    EXPECT_EQ(mgr.channelCount(), 1u);
}

// Duplicate creates are idempotent
TEST(IpcManagerExtTest, DuplicateCreateMessageQueueIsIdempotent)
{
    IpcManager mgr;
    ASSERT_TRUE(mgr.createMessageQueue("mq", 4, false).isOk());
    ASSERT_TRUE(mgr.createMessageQueue("mq", 8, true).isOk()); // different params
    EXPECT_EQ(mgr.channelCount(), 1u);
}

TEST(IpcManagerExtTest, DuplicateCreateSharedMemoryIsIdempotent)
{
    IpcManager mgr;
    ASSERT_TRUE(mgr.createSharedMemory("sm", 16).isOk());
    ASSERT_TRUE(mgr.createSharedMemory("sm", 32).isOk());
    EXPECT_EQ(mgr.channelCount(), 1u);
}

// Zero-size channels: IpcManager delegates to the channel implementation
// which may or may not reject zero sizes. We verify these don't crash and
// that the channel is either created or an error is returned.
TEST(IpcManagerExtTest, ZeroCapacityPipeDoesNotCrash)
{
    IpcManager mgr;
    // createPipe with capacity=0 is implementation-defined; must not crash.
    (void)mgr.createPipe("zero-pipe", 0);
    // If it succeeded, channel count increased; if it failed, it stayed at 0.
    EXPECT_LE(mgr.channelCount(), 1u);
}

TEST(IpcManagerExtTest, ZeroSizeSharedMemoryDoesNotCrash)
{
    IpcManager mgr;
    (void)mgr.createSharedMemory("zero-shm", 0);
    EXPECT_LE(mgr.channelCount(), 1u);
}

// GetChannel after destroy
TEST(IpcManagerExtTest, GetChannelAfterDestroyReturnsNotFound)
{
    IpcManager mgr;
    ASSERT_TRUE(mgr.createPipe("gone", 8).isOk());
    ASSERT_TRUE(mgr.destroyChannel("gone").isOk());

    auto r = mgr.getChannel("gone");
    ASSERT_TRUE(r.isError());
    EXPECT_EQ(r.errorCode(), ErrorCode::NotFound);
}
