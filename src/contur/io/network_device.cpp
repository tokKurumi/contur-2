/// @file network_device.cpp
/// @brief NetworkDevice implementation — simulated LAN device.

#include "contur/io/network_device.h"

#include <deque>

namespace contur {

    struct NetworkDevice::Impl
    {
        std::size_t capacity;
        std::deque<RegisterValue> buffer;

        explicit Impl(std::size_t capacity)
            : capacity(capacity)
        {}
    };

    NetworkDevice::NetworkDevice(std::size_t bufferCapacity)
        : impl_(std::make_unique<Impl>(bufferCapacity))
    {}

    NetworkDevice::~NetworkDevice() = default;
    NetworkDevice::NetworkDevice(NetworkDevice &&) noexcept = default;
    NetworkDevice &NetworkDevice::operator=(NetworkDevice &&) noexcept = default;

    DeviceId NetworkDevice::id() const noexcept
    {
        return NETWORK_DEVICE_ID;
    }

    std::string_view NetworkDevice::name() const noexcept
    {
        return "Network";
    }

    Result<RegisterValue> NetworkDevice::read()
    {
        if (impl_->buffer.empty())
        {
            return Result<RegisterValue>::error(ErrorCode::BufferEmpty);
        }
        RegisterValue value = impl_->buffer.front();
        impl_->buffer.pop_front();
        return Result<RegisterValue>::ok(value);
    }

    Result<void> NetworkDevice::write(RegisterValue value)
    {
        if (impl_->buffer.size() >= impl_->capacity)
        {
            return Result<void>::error(ErrorCode::BufferFull);
        }
        impl_->buffer.push_back(value);
        return Result<void>::ok();
    }

    bool NetworkDevice::isReady() const noexcept
    {
        return impl_->buffer.size() < impl_->capacity;
    }

    std::size_t NetworkDevice::bufferSize() const noexcept
    {
        return impl_->buffer.size();
    }

    bool NetworkDevice::hasData() const noexcept
    {
        return !impl_->buffer.empty();
    }

} // namespace contur
