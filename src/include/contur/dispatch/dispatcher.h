/// @file dispatcher.h
/// @brief Dispatcher implementation.

#pragma once

#include <memory>

#include "contur/dispatch/i_dispatcher.h"

namespace contur {

    class IClock;
    class IExecutionEngine;
    class IScheduler;
    class IVirtualMemory;

    /// @brief Uniprocessor dispatcher implementation.
    ///
    /// Coordinates scheduler selection, execution engine runs,
    /// and virtual-memory lifecycle for active processes.
    class Dispatcher final : public IDispatcher
    {
        public:
        /// @brief Constructs dispatcher from subsystem references.
        Dispatcher(IScheduler &scheduler, IExecutionEngine &engine, IVirtualMemory &virtualMemory, IClock &clock);
        ~Dispatcher() override;

        Dispatcher(const Dispatcher &) = delete;
        Dispatcher &operator=(const Dispatcher &) = delete;
        Dispatcher(Dispatcher &&) noexcept;
        Dispatcher &operator=(Dispatcher &&) noexcept;

        /// @brief Admits process and initializes memory/scheduler state.
        [[nodiscard]] Result<void> createProcess(std::unique_ptr<ProcessImage> process, Tick currentTick) override;

        /// @brief Executes one dispatch cycle for selected process.
        [[nodiscard]] Result<void> dispatch(std::size_t tickBudget) override;

        /// @brief Advances dispatcher clock by one tick.
        void tick() override;

        /// @brief Number of managed processes.
        [[nodiscard]] std::size_t processCount() const noexcept override;

        /// @brief Checks whether process id is currently managed.
        [[nodiscard]] bool hasProcess(ProcessId pid) const noexcept override;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
