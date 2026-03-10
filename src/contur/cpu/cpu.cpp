/// @file cpu.cpp
/// @brief Cpu implementation — fetch-decode-execute cycle.

#include "contur/cpu/cpu.h"

#include <cassert>

#include "contur/arch/block.h"
#include "contur/arch/instruction.h"
#include "contur/arch/isa.h"
#include "contur/cpu/alu.h"
#include "contur/memory/i_memory.h"

namespace contur {

    struct Cpu::Impl
    {
        IMemory &memory;
        ALU alu;
        RegisterValue flags = 0;

        explicit Impl(IMemory &memory)
            : memory(memory)
        {}

        /// @brief Resolves the second operand based on addressing mode.
        /// state=0: operand is an immediate value.
        /// state=1: operand is a register index → read from regs.
        RegisterValue resolveOperand(const Block &block, const RegisterFile &regs) const noexcept
        {
            if (block.state == 1)
            {
                return regs.get(static_cast<std::uint8_t>(block.operand));
            }
            return block.operand;
        }

        /// @brief Executes an arithmetic/logic instruction through the ALU.
        /// @return Interrupt::Ok on success, Interrupt::DivByZero on division by zero.
        Interrupt executeAluOp(Instruction code, RegisterFile &regs, std::uint8_t dstReg, RegisterValue operandValue)
        {
            RegisterValue dstVal = regs.get(dstReg);
            Result<RegisterValue> result = Result<RegisterValue>::error(ErrorCode::InvalidInstruction);

            switch (code)
            {
            case Instruction::Add:
                result = alu.add(dstVal, operandValue);
                break;
            case Instruction::Sub:
                result = alu.sub(dstVal, operandValue);
                break;
            case Instruction::Mul:
                result = alu.mul(dstVal, operandValue);
                break;
            case Instruction::Div:
                result = alu.div(dstVal, operandValue);
                break;
            case Instruction::And:
                result = alu.bitwiseAnd(dstVal, operandValue);
                break;
            case Instruction::Or:
                result = alu.bitwiseOr(dstVal, operandValue);
                break;
            case Instruction::Xor:
                result = alu.bitwiseXor(dstVal, operandValue);
                break;
            case Instruction::ShiftLeft:
                result = alu.shiftLeft(dstVal, operandValue);
                break;
            case Instruction::ShiftRight:
                result = alu.shiftRight(dstVal, operandValue);
                break;
            default:
                return Interrupt::Error;
            }

            if (result.isError())
            {
                if (result.errorCode() == ErrorCode::DivisionByZero)
                {
                    return Interrupt::DivByZero;
                }
                return Interrupt::Error;
            }

            regs.set(dstReg, result.value());
            return Interrupt::Ok;
        }

        /// @brief Evaluates a conditional jump based on flags and instruction type.
        bool shouldJump(Instruction code) const noexcept
        {
            bool zeroSet = (flags & ALU::ZERO_FLAG) != 0;
            bool signSet = (flags & ALU::SIGN_FLAG) != 0;

            switch (code)
            {
            case Instruction::JumpEqual:
                return zeroSet;
            case Instruction::JumpNotEqual:
                return !zeroSet;
            case Instruction::JumpGreater:
                return !zeroSet && !signSet;
            case Instruction::JumpLess:
                return signSet;
            case Instruction::JumpGreaterEqual:
                return !signSet;
            case Instruction::JumpLessEqual:
                return zeroSet || signSet;
            default:
                return false;
            }
        }

        /// @brief Performs one fetch-decode-execute cycle.
        Interrupt step(RegisterFile &regs)
        {
            RegisterValue pc = regs.pc();
            auto fetchResult = memory.read(static_cast<MemoryAddress>(pc));
            if (fetchResult.isError())
            {
                return Interrupt::Error;
            }
            const Block &block = fetchResult.value();

            // Advance PC (may be overridden by jumps/calls)
            regs.setPc(pc + 1);

            switch (block.code)
            {
            case Instruction::Nop:
                return Interrupt::Ok;

            case Instruction::Mov:
                regs.set(block.reg, resolveOperand(block, regs));
                return Interrupt::Ok;

            case Instruction::Add:
            case Instruction::Sub:
            case Instruction::Mul:
            case Instruction::Div:
            case Instruction::And:
            case Instruction::Or:
            case Instruction::Xor:
            case Instruction::ShiftLeft:
            case Instruction::ShiftRight:
                return executeAluOp(block.code, regs, block.reg, resolveOperand(block, regs));

            case Instruction::Compare: {
                RegisterValue a = regs.get(block.reg);
                RegisterValue b = resolveOperand(block, regs);
                flags = alu.compare(a, b);
                return Interrupt::Ok;
            }

            case Instruction::JumpEqual:
            case Instruction::JumpNotEqual:
            case Instruction::JumpGreater:
            case Instruction::JumpLess:
            case Instruction::JumpGreaterEqual:
            case Instruction::JumpLessEqual:
                if (shouldJump(block.code))
                {
                    regs.setPc(block.operand);
                }
                return Interrupt::Ok;

            case Instruction::Push: {
                RegisterValue sp = regs.sp();
                sp--;
                auto writeResult =
                    memory.write(static_cast<MemoryAddress>(sp), Block{Instruction::Nop, 0, regs.get(block.reg)});
                if (writeResult.isError())
                {
                    return Interrupt::Error;
                }
                regs.setSp(sp);
                return Interrupt::Ok;
            }

            case Instruction::Pop: {
                RegisterValue sp = regs.sp();
                auto readResult = memory.read(static_cast<MemoryAddress>(sp));
                if (readResult.isError())
                {
                    return Interrupt::Error;
                }
                regs.set(block.reg, readResult.value().operand);
                sp++;
                regs.setSp(sp);
                return Interrupt::Ok;
            }

            case Instruction::Call: {
                // Push return address (current PC, already advanced)
                RegisterValue sp = regs.sp();
                sp--;
                auto writeResult = memory.write(static_cast<MemoryAddress>(sp), Block{Instruction::Nop, 0, regs.pc()});
                if (writeResult.isError())
                {
                    return Interrupt::Error;
                }
                regs.setSp(sp);
                // Jump to target
                regs.setPc(block.operand);
                return Interrupt::Ok;
            }

            case Instruction::Return: {
                RegisterValue sp = regs.sp();
                auto readResult = memory.read(static_cast<MemoryAddress>(sp));
                if (readResult.isError())
                {
                    return Interrupt::Error;
                }
                regs.setPc(readResult.value().operand);
                sp++;
                regs.setSp(sp);
                return Interrupt::Ok;
            }

            case Instruction::ReadMemory: {
                MemoryAddress addr = static_cast<MemoryAddress>(resolveOperand(block, regs));
                auto readResult = memory.read(addr);
                if (readResult.isError())
                {
                    return Interrupt::Error;
                }
                regs.set(block.reg, readResult.value().operand);
                return Interrupt::Ok;
            }

            case Instruction::WriteMemory: {
                MemoryAddress addr = static_cast<MemoryAddress>(block.operand);
                auto writeResult = memory.write(addr, Block{Instruction::Nop, 0, regs.get(block.reg)});
                if (writeResult.isError())
                {
                    return Interrupt::Error;
                }
                return Interrupt::Ok;
            }

            case Instruction::Interrupt: {
                Interrupt intCode = static_cast<Interrupt>(block.operand);
                if (intCode == Interrupt::Exit)
                {
                    return Interrupt::Exit;
                }
                if (intCode == Interrupt::SystemCall)
                {
                    return Interrupt::SystemCall;
                }
                if (intCode == Interrupt::DeviceIO)
                {
                    return Interrupt::DeviceIO;
                }
                if (intCode == Interrupt::NetworkIO)
                {
                    return Interrupt::NetworkIO;
                }
                return intCode;
            }

            case Instruction::Halt:
                return Interrupt::Exit;
            }

            return Interrupt::Error;
        }
    };

    Cpu::Cpu(IMemory &memory)
        : impl_(std::make_unique<Impl>(memory))
    {}

    Cpu::~Cpu() = default;

    Cpu::Cpu(Cpu &&) noexcept = default;
    Cpu &Cpu::operator=(Cpu &&) noexcept = default;

    Interrupt Cpu::step(RegisterFile &regs)
    {
        return impl_->step(regs);
    }

    void Cpu::reset() noexcept
    {
        impl_->flags = 0;
    }

    RegisterValue Cpu::flags() const noexcept
    {
        return impl_->flags;
    }

} // namespace contur
