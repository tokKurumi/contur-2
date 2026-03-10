/// @file device_manager.h
/// @brief DeviceManager — registry and dispatcher for I/O devices.
///
/// Manages a collection of IDevice instances, allowing registration,
/// lookup, and dispatch of read/write operations by DeviceId.
/// Uses the Composite pattern to treat all devices uniformly.

#pragma once

#include <memory>
#include <vector>

#include "contur/core/error.h"
#include "contur/core/types.h"

#include "contur/io/i_device.h"

namespace contur {

    /// @brief Registry and dispatcher for I/O devices.
    ///
    /// The DeviceManager owns all registered devices (via unique_ptr) and
    /// provides lookup by DeviceId for read/write dispatch.
    class DeviceManager
    {
      public:
        DeviceManager();
        ~DeviceManager();

        // Non-copyable, movable
        DeviceManager(const DeviceManager &) = delete;
        DeviceManager &operator=(const DeviceManager &) = delete;
        DeviceManager(DeviceManager &&) noexcept;
        DeviceManager &operator=(DeviceManager &&) noexcept;

        /// @brief Registers a device. Takes ownership.
        /// @param device The device to register.
        /// @return Success, or AlreadyExists if a device with the same ID is already registered.
        [[nodiscard]] Result<void> registerDevice(std::unique_ptr<IDevice> device);

        /// @brief Removes and destroys a device by ID.
        /// @return Success, or NotFound if no device with the given ID exists.
        [[nodiscard]] Result<void> unregisterDevice(DeviceId id);

        /// @brief Returns a pointer to the device with the given ID, or nullptr.
        [[nodiscard]] IDevice *getDevice(DeviceId id) noexcept;

        /// @brief Returns a const pointer to the device with the given ID, or nullptr.
        [[nodiscard]] const IDevice *getDevice(DeviceId id) const noexcept;

        /// @brief Writes a value to the device with the given ID.
        /// @return Success, or NotFound/DeviceError on failure.
        [[nodiscard]] Result<void> write(DeviceId id, RegisterValue value);

        /// @brief Reads a value from the device with the given ID.
        /// @return The read value, or NotFound/DeviceError on failure.
        [[nodiscard]] Result<RegisterValue> read(DeviceId id);

        /// @brief Returns the number of registered devices.
        [[nodiscard]] std::size_t deviceCount() const noexcept;

        /// @brief Returns true if a device with the given ID is registered.
        [[nodiscard]] bool hasDevice(DeviceId id) const noexcept;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
