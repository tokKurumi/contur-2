/// @file io_types.h
/// @brief Common I/O descriptor and resource-type definitions.

#pragma once

#include <cstdint>

namespace contur {

    /// @brief Unified descriptor handle returned by the kernel I/O layer.
    struct IoDescriptor
    {
        /// @brief Underlying descriptor value.
        std::int32_t value = -1;

        /// @brief Returns true when descriptor is valid.
        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return value >= 0;
        }

        /// @brief Equality comparison for descriptor values.
        friend constexpr bool operator==(IoDescriptor lhs, IoDescriptor rhs) noexcept
        {
            return lhs.value == rhs.value;
        }
    };

    /// @brief Resource kinds supported by the kernel I/O layer.
    enum class IoResourceKind : std::uint8_t
    {
        File = 0,
        LanFile = 1,
        SocketStream = 2,
        SocketMessage = 3,
    };

} // namespace contur
