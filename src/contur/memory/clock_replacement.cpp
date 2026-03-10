/// @file clock_replacement.cpp
/// @brief Clock (Second Chance) page replacement implementation.

#include "contur/memory/clock_replacement.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

namespace contur {

    struct ClockReplacement::Impl
    {
        std::vector<FrameId> frames;         // Circular buffer of loaded frames
        std::unordered_set<FrameId> refBits; // Frames with reference bit set
        std::size_t hand = 0;                // Current clock hand position
    };

    ClockReplacement::ClockReplacement()
        : impl_(std::make_unique<Impl>())
    {}

    ClockReplacement::~ClockReplacement() = default;
    ClockReplacement::ClockReplacement(ClockReplacement &&) noexcept = default;
    ClockReplacement &ClockReplacement::operator=(ClockReplacement &&) noexcept = default;

    std::string_view ClockReplacement::name() const noexcept
    {
        return "Clock";
    }

    FrameId ClockReplacement::selectVictim([[maybe_unused]] const PageTable &pageTable)
    {
        if (impl_->frames.empty())
        {
            return INVALID_FRAME;
        }

        // Scan the circular buffer, giving second chances
        while (true)
        {
            FrameId current = impl_->frames[impl_->hand];

            if (impl_->refBits.count(current) == 0)
            {
                // No reference bit — evict this frame
                impl_->frames.erase(impl_->frames.begin() + static_cast<std::ptrdiff_t>(impl_->hand));
                if (!impl_->frames.empty())
                {
                    impl_->hand %= impl_->frames.size();
                }
                else
                {
                    impl_->hand = 0;
                }
                return current;
            }

            // Clear reference bit — give second chance
            impl_->refBits.erase(current);
            impl_->hand = (impl_->hand + 1) % impl_->frames.size();
        }
    }

    void ClockReplacement::onAccess(FrameId frame)
    {
        impl_->refBits.insert(frame);
    }

    void ClockReplacement::onLoad(FrameId frame)
    {
        impl_->frames.push_back(frame);
        impl_->refBits.insert(frame);
    }

    void ClockReplacement::reset()
    {
        impl_->frames.clear();
        impl_->refBits.clear();
        impl_->hand = 0;
    }

} // namespace contur
