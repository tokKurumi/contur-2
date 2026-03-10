/// @file i_memory.h
/// @brief IMemory interface — abstract linear addressable memory.

#pragma once

#include "contur/arch/block.h"
#include "contur/core/error.h"
#include "contur/core/types.h"

namespace contur {

    /// @brief Abstract interface for linear addressable memory.
    ///
    /// Provides read/write access to Block-sized cells indexed by MemoryAddress.
    /// Implementations include PhysicalMemory (RAM simulation) and potential
    /// future wrappers (cached memory, protected memory, etc.).
    class IMemory
    {
    public:
        virtual ~IMemory() = default;

        /// @brief Reads a Block from the given address.
        /// @param address The memory address to read from.
        /// @return The Block at the address, or an error if the address is invalid.
        [[nodiscard]] virtual Result<Block> read(MemoryAddress address) const = 0;

        /// @brief Writes a Block to the given address.
        /// @param address The memory address to write to.
        /// @param block The Block to store.
        /// @return Success or an error if the address is invalid.
        [[nodiscard]] virtual Result<void> write(MemoryAddress address, const Block& block) = 0;

        /// @brief Returns the total number of addressable cells.
        [[nodiscard]] virtual std::size_t size() const noexcept = 0;

        /// @brief Clears all memory cells to default (Nop) blocks.
        virtual void clear() = 0;
    };

} // namespace contur
