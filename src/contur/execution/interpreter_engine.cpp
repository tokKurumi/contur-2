/// @file interpreter_engine.cpp
/// @brief InterpreterEngine implementation — step-by-step bytecode execution.

#include "contur/execution/interpreter_engine.h"

#include <unordered_set>

#include "contur/arch/interrupt.h"
#include "contur/cpu/i_cpu.h"
#include "contur/memory/i_memory.h"
#include "contur/process/process_image.h"

namespace contur {

    struct InterpreterEngine::Impl
    {
        ICPU &cpu;
        IMemory &memory;
        std::unordered_set<ProcessId> haltedPids;

        Impl(ICPU &cpu, IMemory &memory)
            : cpu(cpu)
            , memory(memory)
        {}

        /// @brief Loads the process's code segment into simulated memory at address 0.
        ///
        /// The code is always loaded at the base of memory (address 0).
        /// The PC is expected to be an offset into this code segment.
        /// @return true if loading succeeded, false if the code doesn't fit in memory.
        bool loadCode(const ProcessImage &process)
        {
            const auto &code = process.code();
            constexpr MemoryAddress BASE_ADDR = 0;

            // Check that the code fits in memory
            if (code.size() > memory.size())
            {
                return false;
            }

            for (std::size_t i = 0; i < code.size(); ++i)
            {
                auto result = memory.write(BASE_ADDR + static_cast<MemoryAddress>(i), code[i]);
                if (result.isError())
                {
                    return false;
                }
            }
            return true;
        }

        /// @brief Executes the process for up to tickBudget CPU steps.
        ExecutionResult run(ProcessImage &process, std::size_t tickBudget)
        {
            ProcessId pid = process.id();

            // Check if this process was forcibly halted
            if (haltedPids.count(pid) > 0)
            {
                haltedPids.erase(pid);
                return ExecutionResult::halted(pid, 0);
            }

            // Load code into memory
            if (!loadCode(process))
            {
                return ExecutionResult::error(pid, 0, Interrupt::Error);
            }

            RegisterFile &regs = process.registers();
            std::size_t ticksConsumed = 0;

            for (std::size_t tick = 0; tick < tickBudget; ++tick)
            {
                // Check for halt between steps
                if (haltedPids.count(pid) > 0)
                {
                    haltedPids.erase(pid);
                    return ExecutionResult::halted(pid, ticksConsumed);
                }

                Interrupt result = cpu.step(regs);
                ++ticksConsumed;

                switch (result)
                {
                case Interrupt::Ok:
                    // Normal execution — continue to next tick
                    break;

                case Interrupt::Exit:
                    return ExecutionResult::exited(pid, ticksConsumed);

                case Interrupt::DivByZero:
                    return ExecutionResult::error(pid, ticksConsumed, Interrupt::DivByZero);

                case Interrupt::Error:
                    return ExecutionResult::error(pid, ticksConsumed, Interrupt::Error);

                case Interrupt::SystemCall:
                    return ExecutionResult::interrupted(pid, ticksConsumed, Interrupt::SystemCall);

                case Interrupt::DeviceIO:
                    return ExecutionResult::interrupted(pid, ticksConsumed, Interrupt::DeviceIO);

                case Interrupt::NetworkIO:
                    return ExecutionResult::interrupted(pid, ticksConsumed, Interrupt::NetworkIO);

                case Interrupt::PageFault:
                    return ExecutionResult::interrupted(pid, ticksConsumed, Interrupt::PageFault);

                case Interrupt::Timer:
                    return ExecutionResult::interrupted(pid, ticksConsumed, Interrupt::Timer);

                default:
                    // Unknown interrupt — treat as error
                    return ExecutionResult::error(pid, ticksConsumed, result);
                }
            }

            // Budget exhausted — preemption
            return ExecutionResult::budgetExhausted(pid, ticksConsumed);
        }
    };

    InterpreterEngine::InterpreterEngine(ICPU &cpu, IMemory &memory)
        : impl_(std::make_unique<Impl>(cpu, memory))
    {}

    InterpreterEngine::~InterpreterEngine() = default;

    InterpreterEngine::InterpreterEngine(InterpreterEngine &&) noexcept = default;
    InterpreterEngine &InterpreterEngine::operator=(InterpreterEngine &&) noexcept = default;

    ExecutionResult InterpreterEngine::execute(ProcessImage &process, std::size_t tickBudget)
    {
        return impl_->run(process, tickBudget);
    }

    void InterpreterEngine::halt(ProcessId pid)
    {
        impl_->haltedPids.insert(pid);
    }

    std::string_view InterpreterEngine::name() const noexcept
    {
        return "Interpreter";
    }

} // namespace contur
