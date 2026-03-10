/// @file device_manager.cpp
/// @brief DeviceManager implementation — I/O device registry and dispatcher.

#include "contur/io/device_manager.h"

#include <algorithm>

namespace contur {

    struct DeviceManager::Impl
    {
        std::vector<std::unique_ptr<IDevice>> devices;

        IDevice *findDevice(DeviceId id) noexcept
        {
            auto it = std::find_if(devices.begin(), devices.end(), [id](const auto &dev) { return dev->id() == id; });
            if (it != devices.end())
            {
                return it->get();
            }
            return nullptr;
        }

        const IDevice *findDevice(DeviceId id) const noexcept
        {
            auto it = std::find_if(devices.begin(), devices.end(), [id](const auto &dev) { return dev->id() == id; });
            if (it != devices.end())
            {
                return it->get();
            }
            return nullptr;
        }
    };

    DeviceManager::DeviceManager()
        : impl_(std::make_unique<Impl>())
    {}

    DeviceManager::~DeviceManager() = default;
    DeviceManager::DeviceManager(DeviceManager &&) noexcept = default;
    DeviceManager &DeviceManager::operator=(DeviceManager &&) noexcept = default;

    Result<void> DeviceManager::registerDevice(std::unique_ptr<IDevice> device)
    {
        if (!device)
        {
            return Result<void>::error(ErrorCode::InvalidArgument);
        }
        if (impl_->findDevice(device->id()) != nullptr)
        {
            return Result<void>::error(ErrorCode::AlreadyExists);
        }
        impl_->devices.push_back(std::move(device));
        return Result<void>::ok();
    }

    Result<void> DeviceManager::unregisterDevice(DeviceId id)
    {
        auto it = std::find_if(impl_->devices.begin(), impl_->devices.end(), [id](const auto &dev) {
            return dev->id() == id;
        });
        if (it == impl_->devices.end())
        {
            return Result<void>::error(ErrorCode::NotFound);
        }
        impl_->devices.erase(it);
        return Result<void>::ok();
    }

    IDevice *DeviceManager::getDevice(DeviceId id) noexcept
    {
        return impl_->findDevice(id);
    }

    const IDevice *DeviceManager::getDevice(DeviceId id) const noexcept
    {
        return impl_->findDevice(id);
    }

    Result<void> DeviceManager::write(DeviceId id, RegisterValue value)
    {
        IDevice *device = impl_->findDevice(id);
        if (device == nullptr)
        {
            return Result<void>::error(ErrorCode::NotFound);
        }
        if (!device->isReady())
        {
            return Result<void>::error(ErrorCode::DeviceError);
        }
        return device->write(value);
    }

    Result<RegisterValue> DeviceManager::read(DeviceId id)
    {
        IDevice *device = impl_->findDevice(id);
        if (device == nullptr)
        {
            return Result<RegisterValue>::error(ErrorCode::NotFound);
        }
        if (!device->isReady())
        {
            return Result<RegisterValue>::error(ErrorCode::DeviceError);
        }
        return device->read();
    }

    std::size_t DeviceManager::deviceCount() const noexcept
    {
        return impl_->devices.size();
    }

    bool DeviceManager::hasDevice(DeviceId id) const noexcept
    {
        return impl_->findDevice(id) != nullptr;
    }

} // namespace contur
