/// @file mp_dispatcher.h
/// @brief Multiprocessor dispatcher orchestrating multiple child dispatchers.

#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "contur/dispatch/i_dispatcher.h"

namespace contur {

    /// @brief Multiprocessor dispatcher delegating to child dispatchers.
    class MPDispatcher final : public IDispatcher
    {
        public:
        /// @brief Constructs MP dispatcher with worker dispatchers.
        /// @param dispatchers Non-empty list of child dispatchers.
        explicit MPDispatcher(std::vector<std::reference_wrapper<IDispatcher>> dispatchers);
        ~MPDispatcher() override;

        MPDispatcher(const MPDispatcher &) = delete;
        MPDispatcher &operator=(const MPDispatcher &) = delete;
        MPDispatcher(MPDispatcher &&) noexcept;
        MPDispatcher &operator=(MPDispatcher &&) noexcept;

        /// @brief Routes a process to one child dispatcher.
        [[nodiscard]] Result<void> createProcess(std::unique_ptr<ProcessImage> process, Tick currentTick) override;

        /// @brief Runs dispatch cycle across all child dispatchers.
        [[nodiscard]] Result<void> dispatch(std::size_t tickBudget) override;

        /// @brief Terminates process in the child dispatcher that owns it.
        [[nodiscard]] Result<void> terminateProcess(ProcessId pid, Tick currentTick) override;

        /// @brief Ticks all child dispatchers.
        void tick() override;

        /// @brief Aggregated number of processes across children.
        [[nodiscard]] std::size_t processCount() const noexcept override;

        /// @brief Returns true if any child contains process pid.
        [[nodiscard]] bool hasProcess(ProcessId pid) const noexcept override;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
