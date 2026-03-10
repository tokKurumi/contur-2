/// @file i_virtual_memory.h
/// @brief IVirtualMemory interface — virtual address space management.

#pragma once

#include <cstddef>
#include <vector>

#include "contur/core/error.h"
#include "contur/core/types.h"

#include "contur/arch/block.h"

namespace contur {

    /// @brief Abstract interface for virtual memory management.
    ///
    /// Provides slot-based allocation of virtual address spaces for processes.
    /// Each slot represents a contiguous virtual memory region that can be
    /// loaded with a code/data segment (vector of Blocks).
    ///
    /// VirtualMemory sits conceptually above the MMU — it manages which
    /// processes have virtual address spaces and provides bulk load/store
    /// operations, while the MMU handles per-address translation.
    class IVirtualMemory
    {
        public:
        virtual ~IVirtualMemory() = default;

        /// @brief Allocates a virtual memory slot for a process.
        /// @param processId The owning process.
        /// @param size Number of Block cells in the virtual address range.
        /// @return The slot index (base virtual address), or an error.
        [[nodiscard]] virtual Result<MemoryAddress> allocateSlot(ProcessId processId, std::size_t size) = 0;

        /// @brief Frees a previously allocated slot.
        /// @param processId The owning process.
        /// @return Success, or an error if the process has no slot.
        [[nodiscard]] virtual Result<void> freeSlot(ProcessId processId) = 0;

        /// @brief Loads a code/data segment into a process's virtual address space.
        /// @param processId The owning process.
        /// @param data The blocks to load.
        /// @return Success, or an error if the slot is too small or not allocated.
        [[nodiscard]] virtual Result<void> loadSegment(ProcessId processId, const std::vector<Block> &data) = 0;

        /// @brief Reads the entire code/data segment from a process's virtual memory.
        /// @param processId The owning process.
        /// @return The blocks in the slot, or an error.
        [[nodiscard]] virtual Result<std::vector<Block>> readSegment(ProcessId processId) const = 0;

        /// @brief Returns the number of total slots available.
        [[nodiscard]] virtual std::size_t totalSlots() const noexcept = 0;

        /// @brief Returns the number of free (unallocated) slots.
        [[nodiscard]] virtual std::size_t freeSlots() const noexcept = 0;

        /// @brief Checks whether a process has an allocated slot.
        [[nodiscard]] virtual bool hasSlot(ProcessId processId) const noexcept = 0;

        /// @brief Returns the slot size (number of blocks) for a process, or 0 if no slot.
        [[nodiscard]] virtual std::size_t slotSize(ProcessId processId) const noexcept = 0;
    };

} // namespace contur
