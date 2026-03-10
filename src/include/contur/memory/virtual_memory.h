/// @file virtual_memory.h
/// @brief VirtualMemory — manages virtual address slots for processes.

#pragma once

#include <memory>

#include "contur/memory/i_virtual_memory.h"

namespace contur {

    // Forward declarations
    class IMMU;

    /// @brief Concrete virtual memory manager.
    ///
    /// Manages slot allocation and delegates actual memory operations
    /// to the underlying IMMU. Each process gets a dedicated virtual
    /// address range (slot) for its code and data.
    class VirtualMemory final : public IVirtualMemory
    {
      public:
        /// @brief Constructs virtual memory backed by the given MMU.
        /// @param mmu The MMU to use for address translation (non-owning reference).
        /// @param maxSlots Maximum number of concurrent process slots.
        explicit VirtualMemory(IMMU &mmu, std::size_t maxSlots);
        ~VirtualMemory() override;

        // Non-copyable, movable
        VirtualMemory(const VirtualMemory &) = delete;
        VirtualMemory &operator=(const VirtualMemory &) = delete;
        VirtualMemory(VirtualMemory &&) noexcept;
        VirtualMemory &operator=(VirtualMemory &&) noexcept;

        // IVirtualMemory interface
        [[nodiscard]] Result<MemoryAddress> allocateSlot(ProcessId processId, std::size_t size) override;
        [[nodiscard]] Result<void> freeSlot(ProcessId processId) override;
        [[nodiscard]] Result<void> loadSegment(ProcessId processId, const std::vector<Block> &data) override;
        [[nodiscard]] Result<std::vector<Block>> readSegment(ProcessId processId) const override;
        [[nodiscard]] std::size_t totalSlots() const noexcept override;
        [[nodiscard]] std::size_t freeSlots() const noexcept override;
        [[nodiscard]] bool hasSlot(ProcessId processId) const noexcept override;
        [[nodiscard]] std::size_t slotSize(ProcessId processId) const noexcept override;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
