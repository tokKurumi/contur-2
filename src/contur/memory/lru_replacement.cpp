/// @file lru_replacement.cpp
/// @brief LRU page replacement implementation.

#include "contur/memory/lru_replacement.h"

#include <algorithm>
#include <unordered_map>

namespace contur {

    struct LruReplacement::Impl {
        std::unordered_map<FrameId, std::uint64_t> accessTime;
        std::uint64_t clock = 0;
    };

    LruReplacement::LruReplacement() : impl_(std::make_unique<Impl>()) {}

    LruReplacement::~LruReplacement() = default;
    LruReplacement::LruReplacement(LruReplacement&&) noexcept = default;
    LruReplacement& LruReplacement::operator=(LruReplacement&&) noexcept = default;

    std::string_view LruReplacement::name() const noexcept
    {
        return "LRU";
    }

    FrameId LruReplacement::selectVictim([[maybe_unused]] const PageTable& pageTable)
    {
        if (impl_->accessTime.empty()) {
            return INVALID_FRAME;
        }

        auto it = std::min_element(
            impl_->accessTime.begin(), impl_->accessTime.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

        FrameId victim = it->first;
        impl_->accessTime.erase(it);
        return victim;
    }

    void LruReplacement::onAccess(FrameId frame)
    {
        impl_->accessTime[frame] = ++impl_->clock;
    }

    void LruReplacement::onLoad(FrameId frame)
    {
        impl_->accessTime[frame] = ++impl_->clock;
    }

    void LruReplacement::reset()
    {
        impl_->accessTime.clear();
        impl_->clock = 0;
    }

} // namespace contur
