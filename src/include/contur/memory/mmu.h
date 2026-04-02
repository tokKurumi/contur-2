/// @file mmu.h
/// @brief MMU implementation — translates virtual addresses to physical addresses.

#pragma once

#include <memory>

#include "contur/memory/i_mmu.h"
#include "contur/memory/i_page_replacement.h"

namespace contur {

    // Forward declarations
    class IMemory;
    class ITracer;

    /// @brief Concrete Memory Management Unit implementation.
    ///
    /// Manages per-process page tables, translates virtual addresses to
    /// physical frame addresses, handles frame allocation/deallocation,
    /// and delegates page replacement decisions to an IPageReplacementPolicy.
    ///
    /// The MMU operates on top of an IMemory instance (physical memory)
    /// and maintains a simulated swap space for pages evicted from RAM.
    class Mmu final : public IMMU
    {
        public:
        /// @brief Constructs an MMU backed by the given physical memory.
        /// @param memory The physical memory to manage (non-owning reference).
        /// @param replacementPolicy The page replacement policy to use.
        explicit Mmu(IMemory &memory, std::unique_ptr<IPageReplacementPolicy> replacementPolicy, ITracer &tracer);

        /// @brief Destroys MMU and associated paging state.
        ~Mmu() override;

        // Non-copyable, movable
        Mmu(const Mmu &) = delete;
        Mmu &operator=(const Mmu &) = delete;
        /// @brief Move-constructs MMU state.
        Mmu(Mmu &&) noexcept;

        /// @brief Move-assigns MMU state.
        Mmu &operator=(Mmu &&) noexcept;

        // IMMU interface
        /// @copydoc IMMU::read
        [[nodiscard]] Result<Block> read(ProcessId processId, MemoryAddress virtualAddress) const override;

        /// @copydoc IMMU::write
        [[nodiscard]] Result<void>
        write(ProcessId processId, MemoryAddress virtualAddress, const Block &block) override;

        /// @copydoc IMMU::allocate
        [[nodiscard]] Result<MemoryAddress> allocate(ProcessId processId, std::size_t pageCount) override;

        /// @copydoc IMMU::deallocate
        [[nodiscard]] Result<void> deallocate(ProcessId processId) override;

        /// @copydoc IMMU::swapIn
        [[nodiscard]] Result<void> swapIn(ProcessId processId, MemoryAddress virtualAddress) override;

        /// @copydoc IMMU::swapOut
        [[nodiscard]] Result<void> swapOut(ProcessId processId, MemoryAddress virtualAddress) override;

        /// @copydoc IMMU::totalFrames
        [[nodiscard]] std::size_t totalFrames() const noexcept override;

        /// @copydoc IMMU::freeFrames
        [[nodiscard]] std::size_t freeFrames() const noexcept override;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
