/// @file optimal_replacement.cpp
/// @brief Optimal (Belady's) page replacement implementation.

#include "contur/memory/optimal_replacement.h"

#include <algorithm>
#include <unordered_set>

namespace contur {

    struct OptimalReplacement::Impl {
        std::vector<FrameId> futureAccesses;
        std::size_t currentIndex = 0;
        std::unordered_set<FrameId> loadedFrames;
    };

    OptimalReplacement::OptimalReplacement(std::vector<FrameId> futureAccesses)
        : impl_(std::make_unique<Impl>())
    {
        impl_->futureAccesses = std::move(futureAccesses);
    }

    OptimalReplacement::~OptimalReplacement() = default;
    OptimalReplacement::OptimalReplacement(OptimalReplacement&&) noexcept = default;
    OptimalReplacement& OptimalReplacement::operator=(OptimalReplacement&&) noexcept = default;

    std::string_view OptimalReplacement::name() const noexcept
    {
        return "Optimal";
    }

    FrameId OptimalReplacement::selectVictim([[maybe_unused]] const PageTable& pageTable)
    {
        if (impl_->loadedFrames.empty()) {
            return INVALID_FRAME;
        }

        // For each loaded frame, find when it will next be accessed
        FrameId victim = INVALID_FRAME;
        std::size_t farthestUse = 0;

        for (FrameId frame : impl_->loadedFrames) {
            // Find next future access for this frame
            auto it = std::find(impl_->futureAccesses.begin() +
                                    static_cast<std::ptrdiff_t>(impl_->currentIndex),
                                impl_->futureAccesses.end(), frame);

            if (it == impl_->futureAccesses.end()) {
                // Frame is never accessed again — ideal victim
                victim = frame;
                break;
            }

            auto distance =
                static_cast<std::size_t>(it - impl_->futureAccesses.begin()) - impl_->currentIndex;
            if (distance > farthestUse || victim == INVALID_FRAME) {
                farthestUse = distance;
                victim = frame;
            }
        }

        impl_->loadedFrames.erase(victim);
        return victim;
    }

    void OptimalReplacement::onAccess([[maybe_unused]] FrameId frame)
    {
        ++impl_->currentIndex;
    }

    void OptimalReplacement::onLoad(FrameId frame)
    {
        impl_->loadedFrames.insert(frame);
    }

    void OptimalReplacement::reset()
    {
        impl_->currentIndex = 0;
        impl_->loadedFrames.clear();
    }

} // namespace contur
