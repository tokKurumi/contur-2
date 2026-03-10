/// @file test_device_manager.cpp
/// @brief Unit tests for DeviceManager and I/O devices.

#include <gtest/gtest.h>

#include "contur/io/console_device.h"
#include "contur/io/device_manager.h"
#include "contur/io/network_device.h"

using namespace contur;

// Helper: A simple test device for controlled testing

class TestDevice final : public IDevice
{
  public:
    explicit TestDevice(DeviceId deviceId, bool ready = true)
        : deviceId_(deviceId)
        , ready_(ready)
    {}

    [[nodiscard]] DeviceId id() const noexcept override
    {
        return deviceId_;
    }
    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "TestDevice";
    }

    [[nodiscard]] Result<RegisterValue> read() override
    {
        return Result<RegisterValue>::ok(lastWritten_);
    }

    [[nodiscard]] Result<void> write(RegisterValue value) override
    {
        lastWritten_ = value;
        writeCount_++;
        return Result<void>::ok();
    }

    [[nodiscard]] bool isReady() const noexcept override
    {
        return ready_;
    }

    RegisterValue lastWritten() const
    {
        return lastWritten_;
    }
    int writeCount() const
    {
        return writeCount_;
    }
    void setReady(bool ready)
    {
        ready_ = ready;
    }

  private:
    DeviceId deviceId_;
    bool ready_;
    RegisterValue lastWritten_ = 0;
    int writeCount_ = 0;
};

// DeviceManager tests

TEST(DeviceManagerTest, InitiallyEmpty)
{
    DeviceManager mgr;
    EXPECT_EQ(mgr.deviceCount(), 0u);
}

TEST(DeviceManagerTest, RegisterDevice)
{
    DeviceManager mgr;
    auto result = mgr.registerDevice(std::make_unique<TestDevice>(1));
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(mgr.deviceCount(), 1u);
    EXPECT_TRUE(mgr.hasDevice(1));
}

TEST(DeviceManagerTest, RegisterMultipleDevices)
{
    DeviceManager mgr;
    ASSERT_TRUE(mgr.registerDevice(std::make_unique<TestDevice>(1)).isOk());
    ASSERT_TRUE(mgr.registerDevice(std::make_unique<TestDevice>(2)).isOk());
    ASSERT_TRUE(mgr.registerDevice(std::make_unique<TestDevice>(3)).isOk());
    EXPECT_EQ(mgr.deviceCount(), 3u);
}

TEST(DeviceManagerTest, RegisterDuplicateIdFails)
{
    DeviceManager mgr;
    ASSERT_TRUE(mgr.registerDevice(std::make_unique<TestDevice>(1)).isOk());
    auto result = mgr.registerDevice(std::make_unique<TestDevice>(1));
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::AlreadyExists);
    EXPECT_EQ(mgr.deviceCount(), 1u);
}

TEST(DeviceManagerTest, RegisterNullptrFails)
{
    DeviceManager mgr;
    auto result = mgr.registerDevice(nullptr);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidArgument);
}

TEST(DeviceManagerTest, UnregisterDevice)
{
    DeviceManager mgr;
    ASSERT_TRUE(mgr.registerDevice(std::make_unique<TestDevice>(1)).isOk());
    ASSERT_TRUE(mgr.unregisterDevice(1).isOk());
    EXPECT_EQ(mgr.deviceCount(), 0u);
    EXPECT_FALSE(mgr.hasDevice(1));
}

TEST(DeviceManagerTest, UnregisterNonExistentFails)
{
    DeviceManager mgr;
    auto result = mgr.unregisterDevice(99);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST(DeviceManagerTest, GetDevice)
{
    DeviceManager mgr;
    ASSERT_TRUE(mgr.registerDevice(std::make_unique<TestDevice>(5)).isOk());
    auto *dev = mgr.getDevice(5);
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->id(), 5);
    EXPECT_EQ(dev->name(), "TestDevice");
}

TEST(DeviceManagerTest, GetDeviceReturnsNullForUnknown)
{
    DeviceManager mgr;
    EXPECT_EQ(mgr.getDevice(99), nullptr);
}

TEST(DeviceManagerTest, GetDeviceConst)
{
    DeviceManager mgr;
    ASSERT_TRUE(mgr.registerDevice(std::make_unique<TestDevice>(5)).isOk());
    const auto &constMgr = mgr;
    const auto *dev = constMgr.getDevice(5);
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(dev->id(), 5);
}

TEST(DeviceManagerTest, WriteToDevice)
{
    DeviceManager mgr;
    auto testDev = std::make_unique<TestDevice>(1);
    auto *rawPtr = testDev.get();
    ASSERT_TRUE(mgr.registerDevice(std::move(testDev)).isOk());

    ASSERT_TRUE(mgr.write(1, 42).isOk());
    EXPECT_EQ(rawPtr->lastWritten(), 42);
    EXPECT_EQ(rawPtr->writeCount(), 1);
}

TEST(DeviceManagerTest, WriteToUnknownDeviceFails)
{
    DeviceManager mgr;
    auto result = mgr.write(99, 42);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST(DeviceManagerTest, WriteToNotReadyDeviceFails)
{
    DeviceManager mgr;
    ASSERT_TRUE(mgr.registerDevice(std::make_unique<TestDevice>(1, false)).isOk());
    auto result = mgr.write(1, 42);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::DeviceError);
}

TEST(DeviceManagerTest, ReadFromDevice)
{
    DeviceManager mgr;
    ASSERT_TRUE(mgr.registerDevice(std::make_unique<TestDevice>(1)).isOk());

    // Write first so there's something to read
    ASSERT_TRUE(mgr.write(1, 77).isOk());
    auto result = mgr.read(1);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value(), 77);
}

TEST(DeviceManagerTest, ReadFromUnknownDeviceFails)
{
    DeviceManager mgr;
    auto result = mgr.read(99);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST(DeviceManagerTest, HasDeviceReturnsFalseAfterUnregister)
{
    DeviceManager mgr;
    ASSERT_TRUE(mgr.registerDevice(std::make_unique<TestDevice>(1)).isOk());
    EXPECT_TRUE(mgr.hasDevice(1));
    ASSERT_TRUE(mgr.unregisterDevice(1).isOk());
    EXPECT_FALSE(mgr.hasDevice(1));
}

// NetworkDevice tests

TEST(NetworkDeviceTest, WriteAndReadBack)
{
    NetworkDevice net(16);
    ASSERT_TRUE(net.write(42).isOk());
    ASSERT_TRUE(net.write(99).isOk());
    EXPECT_EQ(net.bufferSize(), 2u);

    auto r1 = net.read();
    ASSERT_TRUE(r1.isOk());
    EXPECT_EQ(r1.value(), 42);

    auto r2 = net.read();
    ASSERT_TRUE(r2.isOk());
    EXPECT_EQ(r2.value(), 99);

    EXPECT_EQ(net.bufferSize(), 0u);
}

TEST(NetworkDeviceTest, ReadEmptyBufferFails)
{
    NetworkDevice net;
    auto result = net.read();
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::BufferEmpty);
}

TEST(NetworkDeviceTest, WriteToFullBufferFails)
{
    NetworkDevice net(2);
    ASSERT_TRUE(net.write(1).isOk());
    ASSERT_TRUE(net.write(2).isOk());
    auto result = net.write(3);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::BufferFull);
}

TEST(NetworkDeviceTest, IsReadyWhenNotFull)
{
    NetworkDevice net(2);
    EXPECT_TRUE(net.isReady());
    ASSERT_TRUE(net.write(1).isOk());
    EXPECT_TRUE(net.isReady());
    ASSERT_TRUE(net.write(2).isOk());
    EXPECT_FALSE(net.isReady());
}

TEST(NetworkDeviceTest, HasDataReturnsTrueWhenBufferNonEmpty)
{
    NetworkDevice net;
    EXPECT_FALSE(net.hasData());
    ASSERT_TRUE(net.write(42).isOk());
    EXPECT_TRUE(net.hasData());
}

TEST(NetworkDeviceTest, FIFOOrdering)
{
    NetworkDevice net;
    for (int i = 0; i < 5; ++i)
    {
        ASSERT_TRUE(net.write(i * 10).isOk());
    }
    for (int i = 0; i < 5; ++i)
    {
        auto r = net.read();
        ASSERT_TRUE(r.isOk());
        EXPECT_EQ(r.value(), i * 10);
    }
}

TEST(NetworkDeviceTest, DeviceIdAndName)
{
    NetworkDevice net;
    EXPECT_EQ(net.id(), NetworkDevice::NETWORK_DEVICE_ID);
    EXPECT_EQ(net.name(), "Network");
}

// ConsoleDevice basic tests

TEST(ConsoleDeviceTest, DeviceIdAndName)
{
    ConsoleDevice console;
    EXPECT_EQ(console.id(), ConsoleDevice::CONSOLE_DEVICE_ID);
    EXPECT_EQ(console.name(), "Console");
}

TEST(ConsoleDeviceTest, AlwaysReady)
{
    ConsoleDevice console;
    EXPECT_TRUE(console.isReady());
}

TEST(ConsoleDeviceTest, WriteAndReadBack)
{
    ConsoleDevice console;
    // Redirect or accept stdout output — just test it doesn't crash
    ASSERT_TRUE(console.write(65).isOk()); // 'A'
    auto result = console.read();
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value(), 65);
}

// DeviceManager with real devices

TEST(DeviceManagerTest, IntegrationWithRealDevices)
{
    DeviceManager mgr;
    ASSERT_TRUE(mgr.registerDevice(std::make_unique<NetworkDevice>(32)).isOk());

    // Write 3 values
    ASSERT_TRUE(mgr.write(NetworkDevice::NETWORK_DEVICE_ID, 10).isOk());
    ASSERT_TRUE(mgr.write(NetworkDevice::NETWORK_DEVICE_ID, 20).isOk());
    ASSERT_TRUE(mgr.write(NetworkDevice::NETWORK_DEVICE_ID, 30).isOk());

    // Read back in order
    auto r1 = mgr.read(NetworkDevice::NETWORK_DEVICE_ID);
    ASSERT_TRUE(r1.isOk());
    EXPECT_EQ(r1.value(), 10);

    auto r2 = mgr.read(NetworkDevice::NETWORK_DEVICE_ID);
    ASSERT_TRUE(r2.isOk());
    EXPECT_EQ(r2.value(), 20);
}

TEST(DeviceManagerTest, MoveSemantics)
{
    DeviceManager mgr1;
    ASSERT_TRUE(mgr1.registerDevice(std::make_unique<TestDevice>(1)).isOk());
    ASSERT_TRUE(mgr1.registerDevice(std::make_unique<TestDevice>(2)).isOk());

    DeviceManager mgr2(std::move(mgr1));
    EXPECT_EQ(mgr2.deviceCount(), 2u);
    EXPECT_TRUE(mgr2.hasDevice(1));
    EXPECT_TRUE(mgr2.hasDevice(2));
}
