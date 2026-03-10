/// @file physical_memory.cpp
/// @brief PhysicalMemory implementation.

#include "contur/memory/physical_memory.h"

#include <vector>

namespace contur {

    struct PhysicalMemory::Impl
    {
        std::vector<Block> cells;

        explicit Impl(std::size_t cellCount)
            : cells(cellCount)
        {}
    };

    PhysicalMemory::PhysicalMemory(std::size_t cellCount)
        : impl_(std::make_unique<Impl>(cellCount))
    {}

    PhysicalMemory::~PhysicalMemory() = default;
    PhysicalMemory::PhysicalMemory(PhysicalMemory &&) noexcept = default;
    PhysicalMemory &PhysicalMemory::operator=(PhysicalMemory &&) noexcept = default;

    Result<Block> PhysicalMemory::read(MemoryAddress address) const
    {
        if (address >= impl_->cells.size())
        {
            return Result<Block>::error(ErrorCode::InvalidAddress);
        }
        return Result<Block>::ok(impl_->cells[address]);
    }

    Result<void> PhysicalMemory::write(MemoryAddress address, const Block &block)
    {
        if (address >= impl_->cells.size())
        {
            return Result<void>::error(ErrorCode::InvalidAddress);
        }
        impl_->cells[address] = block;
        return Result<void>::ok();
    }

    std::size_t PhysicalMemory::size() const noexcept
    {
        return impl_->cells.size();
    }

    void PhysicalMemory::clear()
    {
        std::fill(impl_->cells.begin(), impl_->cells.end(), Block{});
    }

    Result<void> PhysicalMemory::writeRange(MemoryAddress startAddress, const std::vector<Block> &blocks)
    {
        if (static_cast<std::size_t>(startAddress) + blocks.size() > impl_->cells.size())
        {
            return Result<void>::error(ErrorCode::InvalidAddress);
        }
        for (std::size_t i = 0; i < blocks.size(); ++i)
        {
            impl_->cells[startAddress + i] = blocks[i];
        }
        return Result<void>::ok();
    }

    Result<std::vector<Block>> PhysicalMemory::readRange(MemoryAddress startAddress, std::size_t count) const
    {
        if (static_cast<std::size_t>(startAddress) + count > impl_->cells.size())
        {
            return Result<std::vector<Block>>::error(ErrorCode::InvalidAddress);
        }
        std::vector<Block> result(impl_->cells.begin() + startAddress, impl_->cells.begin() + startAddress + count);
        return Result<std::vector<Block>>::ok(std::move(result));
    }

    Result<void> PhysicalMemory::clearRange(MemoryAddress startAddress, std::size_t count)
    {
        if (static_cast<std::size_t>(startAddress) + count > impl_->cells.size())
        {
            return Result<void>::error(ErrorCode::InvalidAddress);
        }
        std::fill_n(impl_->cells.begin() + startAddress, count, Block{});
        return Result<void>::ok();
    }

} // namespace contur
