/// @file console_device.h
/// @brief ConsoleDevice — stdout output device simulation.
///
/// Writes RegisterValue data to standard output (as characters or integers).
/// Always ready. Read returns the last written value (simple echo buffer).

#pragma once

#include <memory>

#include "contur/io/i_device.h"

namespace contur {

    /// @brief Console output device.
    ///
    /// Simulates a terminal/console device. Writes are sent to stdout.
    /// Reads return the last written value (echo mode).
    class ConsoleDevice final : public IDevice
    {
      public:
        /// @brief Default console device ID.
        static constexpr DeviceId CONSOLE_DEVICE_ID = 1;

        ConsoleDevice();
        ~ConsoleDevice() override;

        // Non-copyable, movable
        ConsoleDevice(const ConsoleDevice &) = delete;
        ConsoleDevice &operator=(const ConsoleDevice &) = delete;
        ConsoleDevice(ConsoleDevice &&) noexcept;
        ConsoleDevice &operator=(ConsoleDevice &&) noexcept;

        [[nodiscard]] DeviceId id() const noexcept override;
        [[nodiscard]] std::string_view name() const noexcept override;
        [[nodiscard]] Result<RegisterValue> read() override;
        [[nodiscard]] Result<void> write(RegisterValue value) override;
        [[nodiscard]] bool isReady() const noexcept override;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
