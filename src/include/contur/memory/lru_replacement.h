/// @file lru_replacement.h
/// @brief LRU page replacement policy — evicts the least recently used page.

#pragma once

#include "contur/memory/i_page_replacement.h"

#include <memory>

namespace contur {

    /// @brief Least Recently Used page replacement.
    ///
    /// Tracks access order via timestamps. On eviction, selects the frame
    /// that has not been accessed for the longest time.
    class LruReplacement final : public IPageReplacementPolicy
    {
    public:
        LruReplacement();
        ~LruReplacement() override;

        LruReplacement(const LruReplacement&) = delete;
        LruReplacement& operator=(const LruReplacement&) = delete;
        LruReplacement(LruReplacement&&) noexcept;
        LruReplacement& operator=(LruReplacement&&) noexcept;

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
