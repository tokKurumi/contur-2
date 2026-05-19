/// @file test_console_device.cpp
/// @brief Unit tests for ConsoleDevice — verifies IDevice contract,
///        last-write-echo read semantics, and printable/non-printable rendering.

#include <iostream>
#include <memory>
#include <sstream>

#include <gtest/gtest.h>

#include "contur/io/console_device.h"
#include "contur/io/i_device.h"

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

TEST(ConsoleDeviceTest, IdMatchesConstant)
{
    ConsoleDevice device;
    EXPECT_EQ(device.id(), ConsoleDevice::CONSOLE_DEVICE_ID);
    EXPECT_EQ(device.id(), 1u);
}

TEST(ConsoleDeviceTest, NameIsConsole)
{
    ConsoleDevice device;
    EXPECT_EQ(device.name(), "Console");
}

TEST(ConsoleDeviceTest, IsReadyAlwaysTrue)
{
    ConsoleDevice device;
    EXPECT_TRUE(device.isReady());
}

TEST(ConsoleDeviceTest, InitialReadReturnsZero)
{
    ConsoleDevice device;
    auto result = device.read();
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value(), 0u);
}

TEST(ConsoleDeviceTest, WriteSucceedsAndUpdatesEchoBuffer)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleDevice device;
    ASSERT_TRUE(device.write(42).isOk());

    auto read = device.read();
    ASSERT_TRUE(read.isOk());
    EXPECT_EQ(read.value(), 42u);
}

TEST(ConsoleDeviceTest, PrintableValueWritesAsCharacter)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleDevice device;
    ASSERT_TRUE(device.write(static_cast<RegisterValue>('Q')).isOk());
    EXPECT_EQ(captured.str(), "Q");
}

TEST(ConsoleDeviceTest, NonPrintableValueWritesAsInteger)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleDevice device;
    ASSERT_TRUE(device.write(31).isOk());  // below 32 (space)
    ASSERT_TRUE(device.write(127).isOk()); // DEL
    ASSERT_TRUE(device.write(255).isOk()); // out of ascii
    EXPECT_NE(captured.str().find("31"), std::string::npos);
    EXPECT_NE(captured.str().find("127"), std::string::npos);
    EXPECT_NE(captured.str().find("255"), std::string::npos);
    // No literal printable character produced for non-printable codes.
    EXPECT_EQ(captured.str().find('\x1f'), std::string::npos);
}

TEST(ConsoleDeviceTest, PrintableBoundariesRenderedAsCharacters)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleDevice device;
    ASSERT_TRUE(device.write(32).isOk());  // ' ' inclusive
    ASSERT_TRUE(device.write(126).isOk()); // '~' inclusive
    const auto output = captured.str();
    EXPECT_NE(output.find(' '), std::string::npos);
    EXPECT_NE(output.find('~'), std::string::npos);
}

TEST(ConsoleDeviceTest, MultipleWritesUpdateEchoBufferToLastValue)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleDevice device;
    ASSERT_TRUE(device.write(1).isOk());
    ASSERT_TRUE(device.write(2).isOk());
    ASSERT_TRUE(device.write(3).isOk());

    auto read = device.read();
    ASSERT_TRUE(read.isOk());
    EXPECT_EQ(read.value(), 3u);
}

TEST(ConsoleDeviceTest, RepeatedReadsDoNotConsume)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleDevice device;
    ASSERT_TRUE(device.write(77).isOk());

    auto first = device.read();
    auto second = device.read();
    ASSERT_TRUE(first.isOk());
    ASSERT_TRUE(second.isOk());
    EXPECT_EQ(first.value(), 77u);
    EXPECT_EQ(second.value(), 77u);
}

TEST(ConsoleDeviceTest, MoveConstructionPreservesEchoState)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    ConsoleDevice original;
    ASSERT_TRUE(original.write(99).isOk());

    ConsoleDevice moved(std::move(original));
    auto read = moved.read();
    ASSERT_TRUE(read.isOk());
    EXPECT_EQ(read.value(), 99u);
}

TEST(ConsoleDeviceTest, ImplementsIDeviceInterface)
{
    std::stringstream captured;
    CoutRedirect redirect(captured.rdbuf());

    std::unique_ptr<IDevice> device = std::make_unique<ConsoleDevice>();
    EXPECT_EQ(device->name(), "Console");
    EXPECT_EQ(device->id(), ConsoleDevice::CONSOLE_DEVICE_ID);
    EXPECT_TRUE(device->isReady());
    ASSERT_TRUE(device->write(7).isOk());
    auto read = device->read();
    ASSERT_TRUE(read.isOk());
    EXPECT_EQ(read.value(), 7u);
}
