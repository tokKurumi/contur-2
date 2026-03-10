/// @file page_table.cpp
/// @brief PageTable implementation.

#include "contur/memory/page_table.h"

#include <algorithm>
#include <vector>

namespace contur {

    struct PageTable::Impl
    {
        std::vector<PageTableEntry> entries;

        explicit Impl(std::size_t pageCount)
            : entries(pageCount)
        {}
    };

    PageTable::PageTable(std::size_t pageCount)
        : impl_(std::make_unique<Impl>(pageCount))
    {}

    PageTable::~PageTable() = default;
    PageTable::PageTable(PageTable &&) noexcept = default;
    PageTable &PageTable::operator=(PageTable &&) noexcept = default;

    Result<void> PageTable::map(std::size_t virtualPage, FrameId frame)
    {
        if (virtualPage >= impl_->entries.size())
        {
            return Result<void>::error(ErrorCode::InvalidAddress);
        }
        auto &entry = impl_->entries[virtualPage];
        entry.frameId = frame;
        entry.present = true;
        entry.dirty = false;
        entry.referenced = true;
        return Result<void>::ok();
    }

    Result<void> PageTable::unmap(std::size_t virtualPage)
    {
        if (virtualPage >= impl_->entries.size())
        {
            return Result<void>::error(ErrorCode::InvalidAddress);
        }
        impl_->entries[virtualPage] = PageTableEntry{};
        return Result<void>::ok();
    }

    Result<FrameId> PageTable::translate(std::size_t virtualPage) const
    {
        if (virtualPage >= impl_->entries.size())
        {
            return Result<FrameId>::error(ErrorCode::InvalidAddress);
        }
        const auto &entry = impl_->entries[virtualPage];
        if (!entry.present)
        {
            return Result<FrameId>::error(ErrorCode::SegmentationFault);
        }
        return Result<FrameId>::ok(entry.frameId);
    }

    Result<PageTableEntry> PageTable::getEntry(std::size_t virtualPage) const
    {
        if (virtualPage >= impl_->entries.size())
        {
            return Result<PageTableEntry>::error(ErrorCode::InvalidAddress);
        }
        return Result<PageTableEntry>::ok(impl_->entries[virtualPage]);
    }

    Result<void> PageTable::setReferenced(std::size_t virtualPage, bool value)
    {
        if (virtualPage >= impl_->entries.size())
        {
            return Result<void>::error(ErrorCode::InvalidAddress);
        }
        impl_->entries[virtualPage].referenced = value;
        return Result<void>::ok();
    }

    Result<void> PageTable::setDirty(std::size_t virtualPage, bool value)
    {
        if (virtualPage >= impl_->entries.size())
        {
            return Result<void>::error(ErrorCode::InvalidAddress);
        }
        impl_->entries[virtualPage].dirty = value;
        return Result<void>::ok();
    }

    Result<void> PageTable::clearReferenced(std::size_t virtualPage)
    {
        return setReferenced(virtualPage, false);
    }

    std::size_t PageTable::pageCount() const noexcept
    {
        return impl_->entries.size();
    }

    std::size_t PageTable::presentCount() const noexcept
    {
        return static_cast<std::size_t>(std::count_if(
            impl_->entries.begin(), impl_->entries.end(), [](const PageTableEntry &e) { return e.present; }
        ));
    }

    void PageTable::clear()
    {
        std::fill(impl_->entries.begin(), impl_->entries.end(), PageTableEntry{});
    }

} // namespace contur
