/// @file virtual_memory.cpp
/// @brief VirtualMemory implementation — slot-based virtual address space manager.

#include "contur/memory/virtual_memory.h"

#include <unordered_map>

#include "contur/memory/i_mmu.h"

namespace contur {

    /// @brief Per-process slot info.
    struct SlotInfo
    {
        ProcessId processId;
        std::size_t size; ///< Number of Block cells in this slot
    };

    struct VirtualMemory::Impl
    {
        IMMU &mmu;
        std::size_t maxSlots;

        /// Maps processId -> slot info
        std::unordered_map<ProcessId, SlotInfo> slots;

        Impl(IMMU &m, std::size_t max)
            : mmu(m)
            , maxSlots(max)
        {}
    };

    VirtualMemory::VirtualMemory(IMMU &mmu, std::size_t maxSlots)
        : impl_(std::make_unique<Impl>(mmu, maxSlots))
    {}

    VirtualMemory::~VirtualMemory() = default;
    VirtualMemory::VirtualMemory(VirtualMemory &&) noexcept = default;
    VirtualMemory &VirtualMemory::operator=(VirtualMemory &&) noexcept = default;

    Result<MemoryAddress> VirtualMemory::allocateSlot(ProcessId processId, std::size_t size)
    {
        if (size == 0)
        {
            return Result<MemoryAddress>::error(ErrorCode::InvalidAddress);
        }

        if (impl_->slots.size() >= impl_->maxSlots)
        {
            return Result<MemoryAddress>::error(ErrorCode::OutOfMemory);
        }

        if (impl_->slots.count(processId) > 0)
        {
            return Result<MemoryAddress>::error(ErrorCode::InvalidPid);
        }

        // Delegate frame allocation to the MMU
        auto allocResult = impl_->mmu.allocate(processId, size);
        if (allocResult.isError())
        {
            return Result<MemoryAddress>::error(allocResult.errorCode());
        }

        impl_->slots[processId] = SlotInfo{processId, size};
        return Result<MemoryAddress>::ok(allocResult.value());
    }

    Result<void> VirtualMemory::freeSlot(ProcessId processId)
    {
        auto it = impl_->slots.find(processId);
        if (it == impl_->slots.end())
        {
            return Result<void>::error(ErrorCode::InvalidPid);
        }

        auto deallocResult = impl_->mmu.deallocate(processId);
        if (deallocResult.isError())
        {
            return Result<void>::error(deallocResult.errorCode());
        }

        impl_->slots.erase(it);
        return Result<void>::ok();
    }

    Result<void> VirtualMemory::loadSegment(ProcessId processId, const std::vector<Block> &data)
    {
        auto it = impl_->slots.find(processId);
        if (it == impl_->slots.end())
        {
            return Result<void>::error(ErrorCode::InvalidPid);
        }

        if (data.size() > it->second.size)
        {
            return Result<void>::error(ErrorCode::OutOfMemory);
        }

        // Write each block via the MMU
        for (std::size_t i = 0; i < data.size(); ++i)
        {
            auto writeResult = impl_->mmu.write(processId, static_cast<MemoryAddress>(i), data[i]);
            if (writeResult.isError())
            {
                return Result<void>::error(writeResult.errorCode());
            }
        }

        return Result<void>::ok();
    }

    Result<std::vector<Block>> VirtualMemory::readSegment(ProcessId processId) const
    {
        auto it = impl_->slots.find(processId);
        if (it == impl_->slots.end())
        {
            return Result<std::vector<Block>>::error(ErrorCode::InvalidPid);
        }

        std::vector<Block> data;
        data.reserve(it->second.size);

        for (std::size_t i = 0; i < it->second.size; ++i)
        {
            auto readResult = impl_->mmu.read(processId, static_cast<MemoryAddress>(i));
            if (readResult.isError())
            {
                return Result<std::vector<Block>>::error(readResult.errorCode());
            }
            data.push_back(readResult.value());
        }

        return Result<std::vector<Block>>::ok(std::move(data));
    }

    std::size_t VirtualMemory::totalSlots() const noexcept
    {
        return impl_->maxSlots;
    }

    std::size_t VirtualMemory::freeSlots() const noexcept
    {
        return impl_->maxSlots - impl_->slots.size();
    }

    bool VirtualMemory::hasSlot(ProcessId processId) const noexcept
    {
        return impl_->slots.count(processId) > 0;
    }

    std::size_t VirtualMemory::slotSize(ProcessId processId) const noexcept
    {
        auto it = impl_->slots.find(processId);
        if (it == impl_->slots.end())
        {
            return 0;
        }
        return it->second.size;
    }

} // namespace contur
