/// @file network_device.h
/// @brief NetworkDevice — simulated LAN output device.
///
/// Simulates a network interface. Written data is stored in an internal
/// buffer (simulating a send buffer). Reads drain the buffer.

#pragma once

#include <memory>

#include "contur/io/i_device.h"

namespace contur {

    /// @brief Simulated network (LAN) device.
    ///
    /// Writes enqueue data into an internal send buffer.
    /// Reads dequeue from a receive buffer (loopback / echo mode for simulation).
    class NetworkDevice final : public IDevice
    {
        public:
        /// @brief Default network device ID.
        static constexpr DeviceId NETWORK_DEVICE_ID = 2;

        /// @brief Constructs a network device with the given buffer capacity.
        /// @param bufferCapacity Maximum number of values in the send/receive buffer.
        explicit NetworkDevice(std::size_t bufferCapacity = 256);
        ~NetworkDevice() override;

        // Non-copyable, movable
        NetworkDevice(const NetworkDevice &) = delete;
        NetworkDevice &operator=(const NetworkDevice &) = delete;
        NetworkDevice(NetworkDevice &&) noexcept;
        NetworkDevice &operator=(NetworkDevice &&) noexcept;

        [[nodiscard]] DeviceId id() const noexcept override;
        [[nodiscard]] std::string_view name() const noexcept override;
        [[nodiscard]] Result<RegisterValue> read() override;
        [[nodiscard]] Result<void> write(RegisterValue value) override;
        [[nodiscard]] bool isReady() const noexcept override;

        /// @brief Returns the number of values currently in the buffer.
        [[nodiscard]] std::size_t bufferSize() const noexcept;

        /// @brief Returns true if the buffer has unread data.
        [[nodiscard]] bool hasData() const noexcept;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
