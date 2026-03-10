/// @file fifo_replacement.cpp
/// @brief FIFO page replacement implementation.

#include "contur/memory/fifo_replacement.h"

#include <deque>

namespace contur {

    struct FifoReplacement::Impl
    {
        std::deque<FrameId> loadOrder;
    };

    FifoReplacement::FifoReplacement()
        : impl_(std::make_unique<Impl>())
    {}

    FifoReplacement::~FifoReplacement() = default;
    FifoReplacement::FifoReplacement(FifoReplacement &&) noexcept = default;
    FifoReplacement &FifoReplacement::operator=(FifoReplacement &&) noexcept = default;

    std::string_view FifoReplacement::name() const noexcept
    {
        return "FIFO";
    }

    FrameId FifoReplacement::selectVictim([[maybe_unused]] const PageTable &pageTable)
    {
        if (impl_->loadOrder.empty())
        {
            return INVALID_FRAME;
        }
        FrameId victim = impl_->loadOrder.front();
        impl_->loadOrder.pop_front();
        return victim;
    }

    void FifoReplacement::onAccess([[maybe_unused]] FrameId frame)
    {
        // FIFO does not care about access order — only load order matters
    }

    void FifoReplacement::onLoad(FrameId frame)
    {
        impl_->loadOrder.push_back(frame);
    }

    void FifoReplacement::reset()
    {
        impl_->loadOrder.clear();
    }

} // namespace contur
