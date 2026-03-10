/// @file register_file.cpp
/// @brief RegisterFile implementation.

#include "contur/arch/register_file.h"

#include <cassert>
#include <iomanip>
#include <sstream>

namespace contur {

    struct RegisterFile::Impl {
        std::array<RegisterValue, REGISTER_COUNT> registers = {};
    };

    RegisterFile::RegisterFile() : impl_(std::make_unique<Impl>()) {}

    RegisterFile::~RegisterFile() = default;

    RegisterFile::RegisterFile(RegisterFile&&) noexcept = default;
    RegisterFile& RegisterFile::operator=(RegisterFile&&) noexcept = default;

    RegisterValue RegisterFile::get(Register reg) const noexcept
    {
        return impl_->registers[static_cast<std::uint8_t>(reg)];
    }

    RegisterValue RegisterFile::get(std::uint8_t index) const noexcept
    {
        assert(index < REGISTER_COUNT && "Register index out of range");
        return impl_->registers[index];
    }

    void RegisterFile::set(Register reg, RegisterValue value) noexcept
    {
        impl_->registers[static_cast<std::uint8_t>(reg)] = value;
    }

    void RegisterFile::set(std::uint8_t index, RegisterValue value) noexcept
    {
        assert(index < REGISTER_COUNT && "Register index out of range");
        impl_->registers[index] = value;
    }

    RegisterValue RegisterFile::pc() const noexcept
    {
        return get(Register::ProgramCounter);
    }

    void RegisterFile::setPc(RegisterValue value) noexcept
    {
        set(Register::ProgramCounter, value);
    }

    RegisterValue RegisterFile::sp() const noexcept
    {
        return get(Register::StackPointer);
    }

    void RegisterFile::setSp(RegisterValue value) noexcept
    {
        set(Register::StackPointer, value);
    }

    void RegisterFile::reset() noexcept
    {
        impl_->registers.fill(0);
    }

    std::array<RegisterValue, REGISTER_COUNT> RegisterFile::snapshot() const noexcept
    {
        return impl_->registers;
    }

    void RegisterFile::restore(const std::array<RegisterValue, REGISTER_COUNT>& values) noexcept
    {
        impl_->registers = values;
    }

    std::string RegisterFile::dump() const
    {
        std::ostringstream oss;
        for (std::uint8_t i = 0; i < REGISTER_COUNT; ++i) {
            auto reg = static_cast<Register>(i);
            oss << std::setw(3) << registerName(reg) << " = " << std::setw(10)
                << impl_->registers[i];
            if (i % 4 == 3) {
                oss << "\n";
            }
            else {
                oss << "  |  ";
            }
        }
        return oss.str();
    }

    std::ostream& operator<<(std::ostream& os, const RegisterFile& rf)
    {
        os << rf.dump();
        return os;
    }

} // namespace contur
