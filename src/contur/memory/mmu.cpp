/// @file mmu.cpp
/// @brief MMU implementation — virtual-to-physical address translation and frame management.

#include "contur/memory/mmu.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "contur/memory/i_memory.h"
#include "contur/memory/page_table.h"

namespace contur {

    struct Mmu::Impl
    {
        IMemory &memory;
        std::unique_ptr<IPageReplacementPolicy> replacementPolicy;

        /// Per-process page tables
        std::unordered_map<ProcessId, PageTable> pageTables;

        /// Free physical frames
        std::unordered_set<FrameId> freeFrames;

        /// Tracks which process owns each frame (for deallocation / swap)
        std::unordered_map<FrameId, ProcessId> frameOwners;

        /// Simulated swap space: frameId -> blocks that were swapped out
        /// Key is a composite of processId and virtualPage for uniqueness
        struct SwapKey
        {
            ProcessId processId;
            std::size_t virtualPage;

            bool operator==(const SwapKey &other) const
            {
                return processId == other.processId && virtualPage == other.virtualPage;
            }
        };

        struct SwapKeyHash
        {
            std::size_t operator()(const SwapKey &key) const
            {
                return std::hash<ProcessId>{}(key.processId) ^ (std::hash<std::size_t>{}(key.virtualPage) << 16);
            }
        };

        std::unordered_map<SwapKey, Block, SwapKeyHash> swapSpace;

        std::size_t totalFrameCount;

        Impl(IMemory &mem, std::unique_ptr<IPageReplacementPolicy> policy)
            : memory(mem)
            , replacementPolicy(std::move(policy))
            , totalFrameCount(mem.size())
        {
            // Initialize all frames as free
            for (std::size_t i = 0; i < totalFrameCount; ++i)
            {
                freeFrames.insert(static_cast<FrameId>(i));
            }
        }

        FrameId allocateFrame(ProcessId pid)
        {
            if (freeFrames.empty())
            {
                return INVALID_FRAME;
            }
            auto it = freeFrames.begin();
            FrameId frame = *it;
            freeFrames.erase(it);
            frameOwners[frame] = pid;
            return frame;
        }

        void freeFrame(FrameId frame)
        {
            frameOwners.erase(frame);
            freeFrames.insert(frame);
        }
    };

    Mmu::Mmu(IMemory &memory, std::unique_ptr<IPageReplacementPolicy> replacementPolicy)
        : impl_(std::make_unique<Impl>(memory, std::move(replacementPolicy)))
    {}

    Mmu::~Mmu() = default;
    Mmu::Mmu(Mmu &&) noexcept = default;
    Mmu &Mmu::operator=(Mmu &&) noexcept = default;

    Result<Block> Mmu::read(ProcessId processId, MemoryAddress virtualAddress) const
    {
        auto it = impl_->pageTables.find(processId);
        if (it == impl_->pageTables.end())
        {
            return Result<Block>::error(ErrorCode::InvalidPid);
        }

        auto translateResult = it->second.translate(virtualAddress);
        if (translateResult.isError())
        {
            return Result<Block>::error(translateResult.errorCode());
        }

        FrameId frame = translateResult.value();

        // Mark page as referenced
        (void)it->second.setReferenced(virtualAddress, true);
        impl_->replacementPolicy->onAccess(frame);

        return impl_->memory.read(static_cast<MemoryAddress>(frame));
    }

    Result<void> Mmu::write(ProcessId processId, MemoryAddress virtualAddress, const Block &block)
    {
        auto it = impl_->pageTables.find(processId);
        if (it == impl_->pageTables.end())
        {
            return Result<void>::error(ErrorCode::InvalidPid);
        }

        auto translateResult = it->second.translate(virtualAddress);
        if (translateResult.isError())
        {
            return Result<void>::error(translateResult.errorCode());
        }

        FrameId frame = translateResult.value();

        // Mark page as referenced and dirty
        (void)it->second.setReferenced(virtualAddress, true);
        (void)it->second.setDirty(virtualAddress, true);
        impl_->replacementPolicy->onAccess(frame);

        return impl_->memory.write(static_cast<MemoryAddress>(frame), block);
    }

    Result<MemoryAddress> Mmu::allocate(ProcessId processId, std::size_t pageCount)
    {
        if (pageCount == 0)
        {
            return Result<MemoryAddress>::error(ErrorCode::InvalidAddress);
        }

        // Create page table for this process if it doesn't exist
        auto [it, inserted] = impl_->pageTables.try_emplace(processId, pageCount);
        if (!inserted)
        {
            // Process already has allocations — could extend, but for simplicity
            // we treat this as an error (reallocate after deallocate)
            return Result<MemoryAddress>::error(ErrorCode::InvalidPid);
        }

        // Allocate physical frames for each page
        for (std::size_t page = 0; page < pageCount; ++page)
        {
            FrameId frame = impl_->allocateFrame(processId);
            if (frame == INVALID_FRAME)
            {
                // Out of physical memory — try page replacement
                frame = impl_->replacementPolicy->selectVictim(it->second);
                if (frame == INVALID_FRAME)
                {
                    // Truly out of memory — rollback what we allocated
                    for (std::size_t rollback = 0; rollback < page; ++rollback)
                    {
                        auto entry = it->second.getEntry(rollback);
                        if (entry.isOk() && entry.value().present)
                        {
                            impl_->freeFrame(entry.value().frameId);
                        }
                    }
                    impl_->pageTables.erase(it);
                    return Result<MemoryAddress>::error(ErrorCode::OutOfMemory);
                }
                // Evict the previous owner's page
                if (auto ownerIt = impl_->frameOwners.find(frame); ownerIt != impl_->frameOwners.end())
                {
                    // Save evicted page to swap space
                    auto readResult = impl_->memory.read(static_cast<MemoryAddress>(frame));
                    if (readResult.isOk())
                    {
                        // Find which virtual page in the owner's page table maps to this frame
                        auto &ownerTable = impl_->pageTables.at(ownerIt->second);
                        for (std::size_t vp = 0; vp < ownerTable.pageCount(); ++vp)
                        {
                            auto vpEntry = ownerTable.getEntry(vp);
                            if (vpEntry.isOk() && vpEntry.value().present && vpEntry.value().frameId == frame)
                            {
                                impl_->swapSpace[{ownerIt->second, vp}] = readResult.value();
                                (void)ownerTable.unmap(vp);
                                break;
                            }
                        }
                    }
                }
                impl_->frameOwners[frame] = processId;
            }

            (void)it->second.map(page, frame);
            impl_->replacementPolicy->onLoad(frame);
        }

        // Return virtual address 0 as the starting address
        return Result<MemoryAddress>::ok(0);
    }

    Result<void> Mmu::deallocate(ProcessId processId)
    {
        auto it = impl_->pageTables.find(processId);
        if (it == impl_->pageTables.end())
        {
            return Result<void>::error(ErrorCode::InvalidPid);
        }

        // Free all physical frames owned by this process
        for (std::size_t page = 0; page < it->second.pageCount(); ++page)
        {
            auto entry = it->second.getEntry(page);
            if (entry.isOk() && entry.value().present)
            {
                impl_->freeFrame(entry.value().frameId);
            }
        }

        // Clear swap space entries for this process
        for (auto swapIt = impl_->swapSpace.begin(); swapIt != impl_->swapSpace.end();)
        {
            if (swapIt->first.processId == processId)
            {
                swapIt = impl_->swapSpace.erase(swapIt);
            }
            else
            {
                ++swapIt;
            }
        }

        impl_->pageTables.erase(it);
        return Result<void>::ok();
    }

    Result<void> Mmu::swapIn(ProcessId processId, MemoryAddress virtualAddress)
    {
        auto tableIt = impl_->pageTables.find(processId);
        if (tableIt == impl_->pageTables.end())
        {
            return Result<void>::error(ErrorCode::InvalidPid);
        }

        auto virtualPage = static_cast<std::size_t>(virtualAddress);
        auto entry = tableIt->second.getEntry(virtualPage);
        if (entry.isError())
        {
            return Result<void>::error(entry.errorCode());
        }

        if (entry.value().present)
        {
            // Already in memory — nothing to do
            return Result<void>::ok();
        }

        // Allocate a frame for the page
        FrameId frame = impl_->allocateFrame(processId);
        if (frame == INVALID_FRAME)
        {
            // Need to evict a page via replacement policy
            frame = impl_->replacementPolicy->selectVictim(tableIt->second);
            if (frame == INVALID_FRAME)
            {
                return Result<void>::error(ErrorCode::OutOfMemory);
            }

            // Save evicted page to swap
            if (auto ownerIt = impl_->frameOwners.find(frame); ownerIt != impl_->frameOwners.end())
            {
                auto readResult = impl_->memory.read(static_cast<MemoryAddress>(frame));
                if (readResult.isOk())
                {
                    auto &ownerTable = impl_->pageTables.at(ownerIt->second);
                    for (std::size_t vp = 0; vp < ownerTable.pageCount(); ++vp)
                    {
                        auto vpEntry = ownerTable.getEntry(vp);
                        if (vpEntry.isOk() && vpEntry.value().present && vpEntry.value().frameId == frame)
                        {
                            impl_->swapSpace[{ownerIt->second, vp}] = readResult.value();
                            (void)ownerTable.unmap(vp);
                            break;
                        }
                    }
                }
            }
            impl_->frameOwners[frame] = processId;
        }

        // Restore data from swap space if available
        Impl::SwapKey key{processId, virtualPage};
        if (auto swapIt = impl_->swapSpace.find(key); swapIt != impl_->swapSpace.end())
        {
            (void)impl_->memory.write(static_cast<MemoryAddress>(frame), swapIt->second);
            impl_->swapSpace.erase(swapIt);
        }

        (void)tableIt->second.map(virtualPage, frame);
        impl_->replacementPolicy->onLoad(frame);
        return Result<void>::ok();
    }

    Result<void> Mmu::swapOut(ProcessId processId, MemoryAddress virtualAddress)
    {
        auto tableIt = impl_->pageTables.find(processId);
        if (tableIt == impl_->pageTables.end())
        {
            return Result<void>::error(ErrorCode::InvalidPid);
        }

        auto virtualPage = static_cast<std::size_t>(virtualAddress);
        auto entry = tableIt->second.getEntry(virtualPage);
        if (entry.isError())
        {
            return Result<void>::error(entry.errorCode());
        }

        if (!entry.value().present)
        {
            // Already swapped out
            return Result<void>::ok();
        }

        FrameId frame = entry.value().frameId;

        // Save page data to swap space
        auto readResult = impl_->memory.read(static_cast<MemoryAddress>(frame));
        if (readResult.isOk())
        {
            impl_->swapSpace[{processId, virtualPage}] = readResult.value();
        }

        // Unmap and free the frame
        (void)tableIt->second.unmap(virtualPage);
        impl_->freeFrame(frame);

        return Result<void>::ok();
    }

    std::size_t Mmu::totalFrames() const noexcept
    {
        return impl_->totalFrameCount;
    }

    std::size_t Mmu::freeFrames() const noexcept
    {
        return impl_->freeFrames.size();
    }

} // namespace contur
