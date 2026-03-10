/// @file process_image.cpp
/// @brief ProcessImage implementation — full in-memory process representation.

#include "contur/process/process_image.h"

#include <cassert>
#include <stdexcept>
#include <utility>

namespace contur {
    struct ProcessImage::Impl
    {
        PCB pcb;
        RegisterFile registers;
        std::vector<Block> code;

        Impl(ProcessId id, std::string name, std::vector<Block> code, Priority priority, Tick arrivalTime)
            : pcb(id, std::move(name), std::move(priority), arrivalTime)
            , code(std::move(code))
        {}
    };

    ProcessImage::ProcessImage(
        ProcessId id, std::string name, std::vector<Block> code, Priority priority, Tick arrivalTime
    )
        : impl_(std::make_unique<Impl>(id, std::move(name), std::move(code), std::move(priority), arrivalTime))
    {}

    ProcessImage::~ProcessImage() = default;

    ProcessImage::ProcessImage(ProcessImage &&) noexcept = default;
    ProcessImage &ProcessImage::operator=(ProcessImage &&) noexcept = default;

    const PCB &ProcessImage::pcb() const noexcept
    {
        return impl_->pcb;
    }

    PCB &ProcessImage::pcb() noexcept
    {
        return impl_->pcb;
    }

    const RegisterFile &ProcessImage::registers() const noexcept
    {
        return impl_->registers;
    }

    RegisterFile &ProcessImage::registers() noexcept
    {
        return impl_->registers;
    }

    const std::vector<Block> &ProcessImage::code() const noexcept
    {
        return impl_->code;
    }

    std::size_t ProcessImage::codeSize() const noexcept
    {
        return impl_->code.size();
    }

    const Block &ProcessImage::readCode(std::size_t offset) const
    {
        assert(offset < impl_->code.size() && "Code offset out of bounds");
        return impl_->code[offset];
    }

    void ProcessImage::setCode(std::vector<Block> newCode)
    {
        impl_->code = std::move(newCode);
    }

    ProcessId ProcessImage::id() const noexcept
    {
        return impl_->pcb.id();
    }

    std::string_view ProcessImage::name() const noexcept
    {
        return impl_->pcb.name();
    }

    ProcessState ProcessImage::state() const noexcept
    {
        return impl_->pcb.state();
    }

    const Priority &ProcessImage::priority() const noexcept
    {
        return impl_->pcb.priority();
    }

} // namespace contur
