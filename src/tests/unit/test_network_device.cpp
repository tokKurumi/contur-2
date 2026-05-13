/// @file test_network_device.cpp
/// @brief Unit tests for NetworkDevice — buffer fill/drain, BufferFull/BufferEmpty
///        contract, FIFO order, readiness, and IDevice polymorphism.

#include <memory>

#include <gtest/gtest.h>

#include "contur/core/error.h"
#include "contur/io/i_device.h"
#include "contur/io/network_device.h"

using namespace contur;

TEST(NetworkDeviceTest, IdMatchesConstant)
{
    NetworkDevice device;
    EXPECT_EQ(device.id(), NetworkDevice::NETWORK_DEVICE_ID);
    EXPECT_EQ(device.id(), 2u);
}

TEST(NetworkDeviceTest, NameIsNetwork)
{
    NetworkDevice device;
    EXPECT_EQ(device.name(), "Network");
}

TEST(NetworkDeviceTest, InitiallyEmpty)
{
    NetworkDevice device;
    EXPECT_EQ(device.bufferSize(), 0u);
    EXPECT_FALSE(device.hasData());
    EXPECT_TRUE(device.isReady());
}

TEST(NetworkDeviceTest, ReadOnEmptyReturnsBufferEmptyError)
{
    NetworkDevice device;
    auto result = device.read();
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::BufferEmpty);
}

TEST(NetworkDeviceTest, WriteIncreasesBufferSize)
{
    NetworkDevice device(8);
    ASSERT_TRUE(device.write(10).isOk());
    EXPECT_EQ(device.bufferSize(), 1u);
    EXPECT_TRUE(device.hasData());
}

TEST(NetworkDeviceTest, ReadFifoOrder)
{
    NetworkDevice device(4);
    ASSERT_TRUE(device.write(1).isOk());
    ASSERT_TRUE(device.write(2).isOk());
    ASSERT_TRUE(device.write(3).isOk());

    auto first = device.read();
    auto second = device.read();
    auto third = device.read();
    ASSERT_TRUE(first.isOk());
    ASSERT_TRUE(second.isOk());
    ASSERT_TRUE(third.isOk());
    EXPECT_EQ(first.value(), 1u);
    EXPECT_EQ(second.value(), 2u);
    EXPECT_EQ(third.value(), 3u);
}

TEST(NetworkDeviceTest, ReadConsumesFromBuffer)
{
    NetworkDevice device(4);
    ASSERT_TRUE(device.write(99).isOk());

    EXPECT_EQ(device.bufferSize(), 1u);
    auto value = device.read();
    ASSERT_TRUE(value.isOk());
    EXPECT_EQ(value.value(), 99u);
    EXPECT_EQ(device.bufferSize(), 0u);
    EXPECT_FALSE(device.hasData());
}

TEST(NetworkDeviceTest, WriteBeyondCapacityFails)
{
    NetworkDevice device(2);
    ASSERT_TRUE(device.write(1).isOk());
    ASSERT_TRUE(device.write(2).isOk());

    EXPECT_FALSE(device.isReady());
    auto overflow = device.write(3);
    ASSERT_TRUE(overflow.isError());
    EXPECT_EQ(overflow.errorCode(), ErrorCode::BufferFull);
}

TEST(NetworkDeviceTest, IsReadyReflectsCapacityHeadroom)
{
    NetworkDevice device(2);
    EXPECT_TRUE(device.isReady());

    ASSERT_TRUE(device.write(1).isOk());
    EXPECT_TRUE(device.isReady());

    ASSERT_TRUE(device.write(2).isOk());
    EXPECT_FALSE(device.isReady());

    ASSERT_TRUE(device.read().isOk());
    EXPECT_TRUE(device.isReady());
}

TEST(NetworkDeviceTest, CapacityZeroAlwaysRejectsWrites)
{
    NetworkDevice device(0);
    EXPECT_FALSE(device.isReady());
    auto write = device.write(1);
    ASSERT_TRUE(write.isError());
    EXPECT_EQ(write.errorCode(), ErrorCode::BufferFull);
}

TEST(NetworkDeviceTest, ReadAfterFullDrainReturnsBufferEmpty)
{
    NetworkDevice device(2);
    ASSERT_TRUE(device.write(1).isOk());
    ASSERT_TRUE(device.write(2).isOk());
    ASSERT_TRUE(device.read().isOk());
    ASSERT_TRUE(device.read().isOk());

    auto extra = device.read();
    ASSERT_TRUE(extra.isError());
    EXPECT_EQ(extra.errorCode(), ErrorCode::BufferEmpty);
}

TEST(NetworkDeviceTest, MoveConstructionPreservesBuffer)
{
    NetworkDevice original(4);
    ASSERT_TRUE(original.write(100).isOk());
    ASSERT_TRUE(original.write(200).isOk());

    NetworkDevice moved(std::move(original));
    EXPECT_EQ(moved.bufferSize(), 2u);

    auto first = moved.read();
    ASSERT_TRUE(first.isOk());
    EXPECT_EQ(first.value(), 100u);
}

TEST(NetworkDeviceTest, InterleavedReadWriteRoundTrip)
{
    NetworkDevice device(2);
    for (RegisterValue i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(device.write(i).isOk());
        auto out = device.read();
        ASSERT_TRUE(out.isOk());
        EXPECT_EQ(out.value(), i);
    }
    EXPECT_FALSE(device.hasData());
}

TEST(NetworkDeviceTest, ImplementsIDeviceInterface)
{
    std::unique_ptr<IDevice> device = std::make_unique<NetworkDevice>(4);
    EXPECT_EQ(device->id(), NetworkDevice::NETWORK_DEVICE_ID);
    EXPECT_EQ(device->name(), "Network");
    EXPECT_TRUE(device->isReady());
    ASSERT_TRUE(device->write(1).isOk());

    auto value = device->read();
    ASSERT_TRUE(value.isOk());
    EXPECT_EQ(value.value(), 1u);
}
