/// @file clock_replacement.h
/// @brief Clock (Second Chance) page replacement policy.

#pragma once

#include <memory>

#include "contur/memory/i_page_replacement.h"

namespace contur {

    /// @brief Clock (Second Chance) page replacement.
    ///
    /// Uses a circular buffer and reference bits. On eviction, scans frames:
    /// - If reference bit is set, clear it and advance (give a "second chance")
    /// - If reference bit is clear, select this frame as victim
    class ClockReplacement final : public IPageReplacementPolicy
    {
      public:
        ClockReplacement();
        ~ClockReplacement() override;

        ClockReplacement(const ClockReplacement &) = delete;
        ClockReplacement &operator=(const ClockReplacement &) = delete;
        ClockReplacement(ClockReplacement &&) noexcept;
        ClockReplacement &operator=(ClockReplacement &&) noexcept;

        [[nodiscard]] std::string_view name() const noexcept override;
        [[nodiscard]] FrameId selectVictim(const PageTable &pageTable) override;
        void onAccess(FrameId frame) override;
        void onLoad(FrameId frame) override;
        void reset() override;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
