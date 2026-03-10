/// @file console_device.cpp
/// @brief ConsoleDevice implementation.

#include "contur/io/console_device.h"

#include <iostream>

namespace contur {

    struct ConsoleDevice::Impl
    {
        RegisterValue lastValue = 0;
    };

    ConsoleDevice::ConsoleDevice()
        : impl_(std::make_unique<Impl>())
    {}

    ConsoleDevice::~ConsoleDevice() = default;
    ConsoleDevice::ConsoleDevice(ConsoleDevice &&) noexcept = default;
    ConsoleDevice &ConsoleDevice::operator=(ConsoleDevice &&) noexcept = default;

    DeviceId ConsoleDevice::id() const noexcept
    {
        return CONSOLE_DEVICE_ID;
    }

    std::string_view ConsoleDevice::name() const noexcept
    {
        return "Console";
    }

    Result<RegisterValue> ConsoleDevice::read()
    {
        return Result<RegisterValue>::ok(impl_->lastValue);
    }

    Result<void> ConsoleDevice::write(RegisterValue value)
    {
        impl_->lastValue = value;
        // Output as character if printable, otherwise as integer
        if (value >= 32 && value < 127)
        {
            std::cout << static_cast<char>(value);
        }
        else
        {
            std::cout << value;
        }
        std::cout.flush();
        return Result<void>::ok();
    }

    bool ConsoleDevice::isReady() const noexcept
    {
        return true;
    }

} // namespace contur
