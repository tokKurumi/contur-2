/// @file i_dispatcher.h
/// @brief IDispatcher interface for process lifecycle orchestration.

#pragma once

#include <cstddef>
#include <memory>

#include "contur/core/error.h"
#include "contur/core/types.h"

namespace contur {

    class ProcessImage;

    /// @brief Interface for process lifecycle dispatch orchestration.
    class IDispatcher
    {
        public:
        virtual ~IDispatcher() = default;

        /// @brief Admits a new process into the system.
        /// @param process Owned process image.
        /// @param currentTick Current simulation tick.
        /// @return Ok on success or an error code.
        [[nodiscard]] virtual Result<void> createProcess(std::unique_ptr<ProcessImage> process, Tick currentTick) = 0;

        /// @brief Runs one scheduling/execution dispatch cycle.
        /// @param tickBudget Time budget for selected process.
        /// @return Ok on success or an error code.
        [[nodiscard]] virtual Result<void> dispatch(std::size_t tickBudget) = 0;

        /// @brief Terminates a managed process immediately.
        /// @param pid Process identifier.
        /// @param currentTick Current simulation tick.
        /// @return Ok on success or NotFound when process is unknown.
        [[nodiscard]] virtual Result<void> terminateProcess(ProcessId pid, Tick currentTick) = 0;

        /// @brief Advances dispatcher-owned simulation time by one tick.
        virtual void tick() = 0;

        /// @brief Returns number of processes tracked by dispatcher.
        [[nodiscard]] virtual std::size_t processCount() const noexcept = 0;

        /// @brief Returns whether process exists.
        [[nodiscard]] virtual bool hasProcess(ProcessId pid) const noexcept = 0;
    };

} // namespace contur
