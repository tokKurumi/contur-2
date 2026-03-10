/// @file i_device.h
/// @brief IDevice — abstract interface for I/O devices.
///
/// All simulated I/O devices (console, network, disk, etc.) implement
/// this interface. The DeviceManager dispatches read/write operations
/// to the appropriate device by DeviceId.

#pragma once

#include <string_view>

#include "contur/core/error.h"
#include "contur/core/types.h"

namespace contur {

    /// @brief Abstract interface for a simulated I/O device.
    ///
    /// Devices are identified by a unique DeviceId and provide
    /// read/write operations on RegisterValue-sized data units.
    class IDevice
    {
      public:
        virtual ~IDevice() = default;

        /// @brief Returns the unique device identifier.
        [[nodiscard]] virtual DeviceId id() const noexcept = 0;

        /// @brief Returns the human-readable device name.
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;

        /// @brief Reads a value from the device.
        /// @return The read value, or an error if the device is not ready or read is unsupported.
        [[nodiscard]] virtual Result<RegisterValue> read() = 0;

        /// @brief Writes a value to the device.
        /// @param value The value to write.
        /// @return Success or an error if the device is not ready or write is unsupported.
        [[nodiscard]] virtual Result<void> write(RegisterValue value) = 0;

        /// @brief Returns true if the device is ready for I/O operations.
        [[nodiscard]] virtual bool isReady() const noexcept = 0;

      protected:
        IDevice() = default;
        IDevice(const IDevice &) = default;
        IDevice &operator=(const IDevice &) = default;
        IDevice(IDevice &&) = default;
        IDevice &operator=(IDevice &&) = default;
    };

} // namespace contur
