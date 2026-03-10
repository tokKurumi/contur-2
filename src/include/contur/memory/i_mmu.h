/// @file i_mmu.h
/// @brief IMMU interface — Memory Management Unit abstraction.

#pragma once

#include <cstddef>

#include "contur/core/error.h"
#include "contur/core/types.h"

#include "contur/arch/block.h"

namespace contur {

    /// @brief Abstract interface for the Memory Management Unit.
    ///
    /// Translates virtual addresses to physical addresses via page tables,
    /// manages frame allocation, and handles page swapping (swap in/out).
    /// The MMU sits between the CPU and physical memory, providing virtual
    /// memory support for process isolation.
    class IMMU
    {
        public:
        virtual ~IMMU() = default;

        /// @brief Reads a Block from a virtual address in a given process's address space.
        /// @param processId The owning process.
        /// @param virtualAddress The virtual address to read from.
        /// @return The Block at the translated physical address, or an error.
        [[nodiscard]] virtual Result<Block> read(ProcessId processId, MemoryAddress virtualAddress) const = 0;

        /// @brief Writes a Block to a virtual address in a given process's address space.
        /// @param processId The owning process.
        /// @param virtualAddress The virtual address to write to.
        /// @param block The Block to store.
        /// @return Success, or an error if translation fails.
        [[nodiscard]] virtual Result<void>
        write(ProcessId processId, MemoryAddress virtualAddress, const Block &block) = 0;

        /// @brief Allocates a range of frames for a process.
        /// @param processId The owning process.
        /// @param pageCount Number of pages (frames) to allocate.
        /// @return The starting virtual address, or an error if out of memory.
        [[nodiscard]] virtual Result<MemoryAddress> allocate(ProcessId processId, std::size_t pageCount) = 0;

        /// @brief Deallocates all frames owned by a process.
        /// @param processId The process whose frames should be freed.
        /// @return Success, or an error if the process has no allocations.
        [[nodiscard]] virtual Result<void> deallocate(ProcessId processId) = 0;

        /// @brief Swaps a page into physical memory.
        /// @param processId The owning process.
        /// @param virtualAddress A virtual address within the target page.
        /// @return Success, or an error if swap fails.
        [[nodiscard]] virtual Result<void> swapIn(ProcessId processId, MemoryAddress virtualAddress) = 0;

        /// @brief Swaps a page out of physical memory (to simulated disk).
        /// @param processId The owning process.
        /// @param virtualAddress A virtual address within the target page.
        /// @return Success, or an error if swap fails.
        [[nodiscard]] virtual Result<void> swapOut(ProcessId processId, MemoryAddress virtualAddress) = 0;

        /// @brief Returns the total number of physical frames managed.
        [[nodiscard]] virtual std::size_t totalFrames() const noexcept = 0;

        /// @brief Returns the number of free (unallocated) physical frames.
        [[nodiscard]] virtual std::size_t freeFrames() const noexcept = 0;
    };

} // namespace contur
