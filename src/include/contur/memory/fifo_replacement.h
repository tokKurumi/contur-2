/// @file fifo_replacement.h
/// @brief FIFO page replacement policy — evicts the oldest loaded page.

#pragma once

#include "contur/memory/i_page_replacement.h"

#include <memory>

namespace contur {

    /// @brief First-In First-Out page replacement.
    ///
    /// Maintains a queue of loaded frames in order of arrival.
    /// On eviction, selects the frame that was loaded earliest.
    class FifoReplacement final : public IPageReplacementPolicy
    {
    public:
        FifoReplacement();
        ~FifoReplacement() override;

        FifoReplacement(const FifoReplacement&) = delete;
        FifoReplacement& operator=(const FifoReplacement&) = delete;
        FifoReplacement(FifoReplacement&&) noexcept;
        FifoReplacement& operator=(FifoReplacement&&) noexcept;

        [[nodiscard]] std::string_view name() const noexcept override;
        [[nodiscard]] FrameId selectVictim(const PageTable& pageTable) override;
        void onAccess(FrameId frame) override;
        void onLoad(FrameId frame) override;
        void reset() override;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
