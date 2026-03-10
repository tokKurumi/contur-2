/// @file i_page_replacement.h
/// @brief IPageReplacementPolicy interface — pluggable page replacement algorithms.

#pragma once

#include <string_view>

#include "contur/core/types.h"

#include "contur/memory/page_table.h"

namespace contur {

    /// @brief Abstract interface for page replacement algorithms.
    ///
    /// The MMU consults this policy to decide which physical frame to evict
    /// when a page fault occurs and no free frames are available.
    /// Implementations: FIFO, LRU, Clock, Optimal.
    class IPageReplacementPolicy
    {
        public:
        virtual ~IPageReplacementPolicy() = default;

        /// @brief Returns the name of the algorithm (e.g., "FIFO", "LRU").
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;

        /// @brief Selects a victim frame to evict.
        /// @param pageTable The current page table state.
        /// @return The FrameId of the frame to evict.
        [[nodiscard]] virtual FrameId selectVictim(const PageTable &pageTable) = 0;

        /// @brief Notifies the policy that a frame was accessed (read or write).
        virtual void onAccess(FrameId frame) = 0;

        /// @brief Notifies the policy that a new page was loaded into a frame.
        virtual void onLoad(FrameId frame) = 0;

        /// @brief Resets the policy's internal state.
        virtual void reset() = 0;
    };

} // namespace contur
