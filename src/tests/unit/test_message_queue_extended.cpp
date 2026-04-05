/// @file test_message_queue_extended.cpp
/// @brief Extended unit tests for MessageQueue — priority tie-breaking,
///        size tracking, send/receive interleaving, large payloads, move
///        semantics, FIFO vs priority mode comparison.

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "contur/core/error.h"

#include "contur/ipc/message_queue.h"

using namespace contur;

namespace {

    Message msg(std::uint32_t type, std::uint32_t priority, unsigned char val)
    {
        return Message{type, priority, {std::byte{val}}};
    }

} // namespace

// Size tracking
TEST(MessageQueueExtTest, SizeIncrementsOnSendAndDecrementsOnReceive)
{
    MessageQueue q("q", 4, false);
    EXPECT_EQ(q.size(), 0u);

    ASSERT_TRUE(q.send(msg(1, 0, 1)).isOk());
    EXPECT_EQ(q.size(), 1u);

    ASSERT_TRUE(q.send(msg(2, 0, 2)).isOk());
    EXPECT_EQ(q.size(), 2u);

    ASSERT_TRUE(q.receive().isOk());
    EXPECT_EQ(q.size(), 1u);

    ASSERT_TRUE(q.receive().isOk());
    EXPECT_EQ(q.size(), 0u);
}

// FIFO order with equal priorities
TEST(MessageQueueExtTest, PriorityModeWithEqualPrioritiesFallsBackToFifo)
{
    MessageQueue q("q", 4, true);

    ASSERT_TRUE(q.send(msg(10, 5, 1)).isOk());
    ASSERT_TRUE(q.send(msg(20, 5, 2)).isOk());
    ASSERT_TRUE(q.send(msg(30, 5, 3)).isOk());

    // All same priority — insertion order must be preserved
    auto a = q.receive();
    auto b = q.receive();
    auto c = q.receive();

    ASSERT_TRUE(a.isOk());
    ASSERT_TRUE(b.isOk());
    ASSERT_TRUE(c.isOk());

    EXPECT_EQ(a.value().type, 10u);
    EXPECT_EQ(b.value().type, 20u);
    EXPECT_EQ(c.value().type, 30u);
}

// Priority mode with mixed priorities
TEST(MessageQueueExtTest, PriorityModeHighestFirstThenDescending)
{
    MessageQueue q("q", 5, true);

    ASSERT_TRUE(q.send(msg(1, 10, 10)).isOk());
    ASSERT_TRUE(q.send(msg(2, 2, 2)).isOk());
    ASSERT_TRUE(q.send(msg(3, 50, 50)).isOk());
    ASSERT_TRUE(q.send(msg(4, 50, 51)).isOk()); // tie with msg 3
    ASSERT_TRUE(q.send(msg(5, 1, 1)).isOk());

    // expected: 3(50), 4(50), 1(10), 2(2), 5(1) — or 4 before 3 if stable
    auto r0 = q.receive();
    ASSERT_TRUE(r0.isOk());
    EXPECT_EQ(r0.value().priority, 50u);

    auto r1 = q.receive();
    ASSERT_TRUE(r1.isOk());
    EXPECT_EQ(r1.value().priority, 50u);

    auto r2 = q.receive();
    ASSERT_TRUE(r2.isOk());
    EXPECT_EQ(r2.value().priority, 10u);

    auto r3 = q.receive();
    ASSERT_TRUE(r3.isOk());
    EXPECT_EQ(r3.value().priority, 2u);

    auto r4 = q.receive();
    ASSERT_TRUE(r4.isOk());
    EXPECT_EQ(r4.value().priority, 1u);
}

// FIFO mode vs priority mode — ordering differs
TEST(MessageQueueExtTest, FifoModeIgnoresPriority)
{
    MessageQueue q("q", 3, false);

    ASSERT_TRUE(q.send(msg(1, 1, 1)).isOk());
    ASSERT_TRUE(q.send(msg(2, 9, 2)).isOk());
    ASSERT_TRUE(q.send(msg(3, 5, 3)).isOk());

    // In FIFO mode, arrival order wins regardless of priority
    auto a = q.receive();
    auto b = q.receive();
    auto c = q.receive();

    ASSERT_TRUE(a.isOk());
    ASSERT_TRUE(b.isOk());
    ASSERT_TRUE(c.isOk());

    EXPECT_EQ(a.value().type, 1u);
    EXPECT_EQ(b.value().type, 2u);
    EXPECT_EQ(c.value().type, 3u);
}

// Interleaved send/receive
TEST(MessageQueueExtTest, InterleavedSendReceivePreservesConsistency)
{
    MessageQueue q("q", 4, false);

    ASSERT_TRUE(q.send(msg(1, 0, 1)).isOk());
    auto r0 = q.receive();
    ASSERT_TRUE(r0.isOk());
    EXPECT_EQ(r0.value().type, 1u);

    ASSERT_TRUE(q.send(msg(2, 0, 2)).isOk());
    ASSERT_TRUE(q.send(msg(3, 0, 3)).isOk());
    auto r1 = q.receive();
    ASSERT_TRUE(r1.isOk());
    EXPECT_EQ(r1.value().type, 2u);

    ASSERT_TRUE(q.send(msg(4, 0, 4)).isOk());
    auto r2 = q.receive();
    auto r3 = q.receive();
    ASSERT_TRUE(r2.isOk());
    ASSERT_TRUE(r3.isOk());
    EXPECT_EQ(r2.value().type, 3u);
    EXPECT_EQ(r3.value().type, 4u);

    EXPECT_EQ(q.size(), 0u);
}

// Large payload
TEST(MessageQueueExtTest, LargePayloadRoundTrip)
{
    MessageQueue q("q", 2, false);
    std::vector<std::byte> payload(256);
    for (std::size_t i = 0; i < 256; ++i)
    {
        payload[i] = static_cast<std::byte>(i);
    }

    Message m;
    m.type = 99;
    m.priority = 0;
    m.payload = payload;

    ASSERT_TRUE(q.send(m).isOk());

    auto r = q.receive();
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value().type, 99u);
    EXPECT_EQ(r.value().payload.size(), 256u);
    for (std::size_t i = 0; i < 256; ++i)
    {
        EXPECT_EQ(r.value().payload[i], static_cast<std::byte>(i)) << "byte " << i;
    }
}

// Move semantics
TEST(MessageQueueExtTest, MoveConstructedQueueOwnsMessages)
{
    MessageQueue src("src", 2, false);
    ASSERT_TRUE(src.send(msg(1, 0, 10)).isOk());

    MessageQueue dst(std::move(src));

    EXPECT_EQ(dst.name(), "src");
    EXPECT_EQ(dst.size(), 1u);

    auto r = dst.receive();
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.value().type, 1u);
}

// maxMessages query
TEST(MessageQueueExtTest, MaxMessagesReflectsConstructorArgument)
{
    MessageQueue q("q", 17, false);
    EXPECT_EQ(q.maxMessages(), 17u);
}

// isPriorityMode query
TEST(MessageQueueExtTest, IsPriorityModeFlagMatchesConstructorArg)
{
    MessageQueue fifo("f", 4, false);
    MessageQueue prio("p", 4, true);

    EXPECT_FALSE(fifo.isPriorityMode());
    EXPECT_TRUE(prio.isPriorityMode());
}
