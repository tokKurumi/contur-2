/// @file page_table.h
/// @brief PageTable — virtual-to-physical frame mapping with status bits.

#pragma once

#include "contur/core/error.h"
#include "contur/core/types.h"

#include <memory>

namespace contur {

    /// @brief Entry in the page table — maps one virtual page to a physical frame.
    struct PageTableEntry {
        FrameId frameId = INVALID_FRAME;  ///< Physical frame number
        bool present = false;             ///< Page is loaded in physical memory
        bool dirty = false;               ///< Page has been modified since load
        bool referenced = false;          ///< Page has been accessed (for Clock algorithm)
    };

    /// @brief Page table mapping virtual page numbers to physical frames.
    ///
    /// Each process has its own PageTable. The MMU uses it to translate
    /// virtual addresses and to decide which pages to swap in/out.
    class PageTable
    {
    public:
        /// @brief Constructs a page table with the given number of virtual pages.
        explicit PageTable(std::size_t pageCount);
        ~PageTable();

        // Non-copyable, movable
        PageTable(const PageTable&) = delete;
        PageTable& operator=(const PageTable&) = delete;
        PageTable(PageTable&&) noexcept;
        PageTable& operator=(PageTable&&) noexcept;

        /// @brief Maps a virtual page to a physical frame.
        /// @return Success or InvalidAddress if virtualPage is out of range.
        [[nodiscard]] Result<void> map(std::size_t virtualPage, FrameId frame);

        /// @brief Unmaps a virtual page (marks as not present, clears frame).
        /// @return Success or InvalidAddress if virtualPage is out of range.
        [[nodiscard]] Result<void> unmap(std::size_t virtualPage);

        /// @brief Translates a virtual page number to a physical frame.
        /// @return The FrameId, or InvalidAddress if not mapped, or SegmentationFault if not present.
        [[nodiscard]] Result<FrameId> translate(std::size_t virtualPage) const;

        /// @brief Returns the entry for a virtual page (read-only).
        [[nodiscard]] Result<PageTableEntry> getEntry(std::size_t virtualPage) const;

        /// @brief Marks a page as referenced (used by Clock/LRU algorithms).
        [[nodiscard]] Result<void> setReferenced(std::size_t virtualPage, bool value);

        /// @brief Marks a page as dirty.
        [[nodiscard]] Result<void> setDirty(std::size_t virtualPage, bool value);

        /// @brief Clears the referenced bit for a virtual page.
        [[nodiscard]] Result<void> clearReferenced(std::size_t virtualPage);

        /// @brief Returns the total number of virtual pages.
        [[nodiscard]] std::size_t pageCount() const noexcept;

        /// @brief Returns the number of pages currently present in memory.
        [[nodiscard]] std::size_t presentCount() const noexcept;

        /// @brief Resets all entries to unmapped state.
        void clear();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
