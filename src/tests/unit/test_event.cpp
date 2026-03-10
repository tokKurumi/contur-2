/// @file test_event.cpp
/// @brief Unit tests for Event<Args...>.

#include "contur/core/event.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace contur;

TEST(EventTest, SubscribeAndEmit)
{
    Event<int> event;
    int received = 0;

    (void)event.subscribe([&received](int val) { received = val; });
    event.emit(42);

    EXPECT_EQ(received, 42);
}

TEST(EventTest, MultipleSubscribers)
{
    Event<int> event;
    int sum = 0;

    (void)event.subscribe([&sum](int val) { sum += val; });
    (void)event.subscribe([&sum](int val) { sum += val * 10; });

    event.emit(5);
    EXPECT_EQ(sum, 55); // 5 + 50
}

TEST(EventTest, Unsubscribe)
{
    Event<int> event;
    int callCount = 0;

    auto id = event.subscribe([&callCount](int) { ++callCount; });
    event.emit(1);
    EXPECT_EQ(callCount, 1);

    bool removed = event.unsubscribe(id);
    EXPECT_TRUE(removed);

    event.emit(1);
    EXPECT_EQ(callCount, 1); // not called again
}

TEST(EventTest, UnsubscribeInvalidId)
{
    Event<int> event;
    EXPECT_FALSE(event.unsubscribe(999));
}

TEST(EventTest, MultipleArguments)
{
    Event<int, std::string> event;
    int receivedId = 0;
    std::string receivedName;

    (void)event.subscribe([&](int id, const std::string& name) {
        receivedId = id;
        receivedName = name;
    });

    event.emit(7, std::string("process_7"));
    EXPECT_EQ(receivedId, 7);
    EXPECT_EQ(receivedName, "process_7");
}

TEST(EventTest, NoArguments)
{
    Event<> event;
    int callCount = 0;

    (void)event.subscribe([&callCount]() { ++callCount; });
    event.emit();
    event.emit();
    event.emit();

    EXPECT_EQ(callCount, 3);
}

TEST(EventTest, SubscriberCount)
{
    Event<int> event;
    EXPECT_EQ(event.subscriberCount(), 0u);

    auto id1 = event.subscribe([](int) {});
    EXPECT_EQ(event.subscriberCount(), 1u);

    auto id2 = event.subscribe([](int) {});
    EXPECT_EQ(event.subscriberCount(), 2u);

    event.unsubscribe(id1);
    EXPECT_EQ(event.subscriberCount(), 1u);

    event.unsubscribe(id2);
    EXPECT_EQ(event.subscriberCount(), 0u);
}

TEST(EventTest, Clear)
{
    Event<int> event;
    (void)event.subscribe([](int) {});
    (void)event.subscribe([](int) {});
    (void)event.subscribe([](int) {});
    EXPECT_EQ(event.subscriberCount(), 3u);

    event.clear();
    EXPECT_EQ(event.subscriberCount(), 0u);
}

TEST(EventTest, EmitWithNoSubscribers)
{
    Event<int> event;
    // Should not crash
    event.emit(42);
}

TEST(EventTest, SubscriptionIdsAreUnique)
{
    Event<int> event;
    auto id1 = event.subscribe([](int) {});
    auto id2 = event.subscribe([](int) {});
    auto id3 = event.subscribe([](int) {});

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

TEST(EventTest, EmitOrder)
{
    Event<int> event;
    std::vector<int> order;

    (void)event.subscribe([&order](int) { order.push_back(1); });
    (void)event.subscribe([&order](int) { order.push_back(2); });
    (void)event.subscribe([&order](int) { order.push_back(3); });

    event.emit(0);

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST(EventTest, MoveConstruction)
{
    Event<int> event;
    int received = 0;
    (void)event.subscribe([&received](int val) { received = val; });

    Event<int> moved(std::move(event));
    moved.emit(99);

    EXPECT_EQ(received, 99);
}
