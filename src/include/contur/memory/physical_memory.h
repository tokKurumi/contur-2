/// @file physical_memory.h
/// @brief PhysicalMemory — RAM simulation backed by std::vector<Block>.

#pragma once

#include <memory>
#include <vector>

#include "contur/memory/i_memory.h"

namespace contur {

    /// @brief Simulated physical RAM — a linear array of Block cells.
    ///
    /// Supports read, write, clear, and bulk operations for
    /// loading/saving code segments (swap in/out).
    class PhysicalMemory final : public IMemory
    {
      public:
        /// @brief Constructs physical memory with the given number of cells.
        /// @param cellCount Total addressable cells (each holds one Block).
        explicit PhysicalMemory(std::size_t cellCount);
        ~PhysicalMemory() override;

        // Non-copyable, movable
        PhysicalMemory(const PhysicalMemory &) = delete;
        PhysicalMemory &operator=(const PhysicalMemory &) = delete;
        PhysicalMemory(PhysicalMemory &&) noexcept;
        PhysicalMemory &operator=(PhysicalMemory &&) noexcept;

        // IMemory interface
        [[nodiscard]] Result<Block> read(MemoryAddress address) const override;
        [[nodiscard]] Result<void> write(MemoryAddress address, const Block &block) override;
        [[nodiscard]] std::size_t size() const noexcept override;
        void clear() override;

        /// @brief Writes a contiguous sequence of blocks starting at the given address.
        /// @param startAddress The base address to begin writing.
        /// @param blocks The blocks to write sequentially.
        /// @return Success or an error if any address is out of range.
        [[nodiscard]] Result<void> writeRange(MemoryAddress startAddress, const std::vector<Block> &blocks);

        /// @brief Reads a contiguous sequence of blocks starting at the given address.
        /// @param startAddress The base address to begin reading.
        /// @param count Number of blocks to read.
        /// @return The blocks, or an error if any address is out of range.
        [[nodiscard]] Result<std::vector<Block>> readRange(MemoryAddress startAddress, std::size_t count) const;

        /// @brief Clears a range of memory cells to default blocks.
        /// @param startAddress The base address to begin clearing.
        /// @param count Number of blocks to clear.
        /// @return Success or an error if any address is out of range.
        [[nodiscard]] Result<void> clearRange(MemoryAddress startAddress, std::size_t count);

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
