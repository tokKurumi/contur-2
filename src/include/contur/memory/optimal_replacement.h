/// @file optimal_replacement.h
/// @brief Optimal (Belady's) page replacement policy — educational only.

#pragma once

#include <memory>
#include <vector>

#include "contur/memory/i_page_replacement.h"

namespace contur {

    /// @brief Optimal page replacement (Belady's algorithm).
    ///
    /// Evicts the page that will not be used for the longest time in the future.
    /// Requires the complete future access sequence to be known in advance.
    /// This is impractical for real systems but serves as a theoretical baseline
    /// for comparing other algorithms' performance.
    class OptimalReplacement final : public IPageReplacementPolicy
    {
      public:
        /// @brief Constructs with a known future access sequence.
        /// @param futureAccesses Ordered list of FrameIds that will be accessed.
        explicit OptimalReplacement(std::vector<FrameId> futureAccesses);
        ~OptimalReplacement() override;

        OptimalReplacement(const OptimalReplacement &) = delete;
        OptimalReplacement &operator=(const OptimalReplacement &) = delete;
        OptimalReplacement(OptimalReplacement &&) noexcept;
        OptimalReplacement &operator=(OptimalReplacement &&) noexcept;

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
