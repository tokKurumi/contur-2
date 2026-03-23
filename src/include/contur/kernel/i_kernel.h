/// @file i_kernel.h
/// @brief IKernel facade interface for top-level kernel operations.

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "contur/core/error.h"
#include "contur/core/types.h"

#include "contur/arch/block.h"
#include "contur/process/priority.h"
#include "contur/syscall/syscall_ids.h"

namespace contur {

    class ISyncPrimitive;
    class ProcessImage;

    /// @brief Configuration payload used to create a process.
    struct ProcessConfig
    {
        /// @brief Optional explicit PID. INVALID_PID requests auto-assignment.
        ProcessId id = INVALID_PID;

        /// @brief Human-readable process name.
        std::string name;

        /// @brief Program code segment.
        std::vector<Block> code;

        /// @brief Initial scheduling priority.
        Priority priority{};

        /// @brief Arrival timestamp override. Defaults to current clock tick.
        Tick arrivalTime = 0;
    };

    /// @brief Lightweight kernel state snapshot for UI and diagnostics.
    struct KernelSnapshot
    {
        /// @brief Current simulation tick.
        Tick currentTick = 0;

        /// @brief Total number of managed processes.
        std::size_t processCount = 0;

        /// @brief Number of processes in the ready queue.
        std::size_t readyCount = 0;

        /// @brief Number of processes in blocked state.
        std::size_t blockedCount = 0;

        /// @brief PID of the currently running process or INVALID_PID.
        ProcessId runningPid = INVALID_PID;

        /// @brief Total number of virtual memory slots.
        std::size_t totalVirtualSlots = 0;

        /// @brief Number of free virtual memory slots.
        std::size_t freeVirtualSlots = 0;
    };

    /// @brief Function signature used for syscall registration.
    using SyscallHandlerFn =
        std::function<Result<RegisterValue>(std::span<const RegisterValue> args, ProcessImage &caller)>;

    /// @brief Top-level kernel facade.
    class IKernel
    {
        public:
        virtual ~IKernel() = default;

        /// @brief Creates and admits a process.
        /// @param config Process configuration payload.
        /// @return Created process ID or error.
        [[nodiscard]] virtual Result<ProcessId> createProcess(const ProcessConfig &config) = 0;

        /// @brief Terminates a process immediately.
        /// @param pid Process identifier.
        /// @return Ok on success or error if process is unknown.
        [[nodiscard]] virtual Result<void> terminateProcess(ProcessId pid) = 0;

        /// @brief Executes one dispatch cycle.
        /// @param tickBudget Optional tick budget. 0 means kernel default.
        /// @return Ok on success or error.
        [[nodiscard]] virtual Result<void> tick(std::size_t tickBudget = 0) = 0;

        /// @brief Executes multiple dispatch cycles.
        /// @param cycles Number of cycles to run.
        /// @param tickBudget Optional budget per cycle. 0 means kernel default.
        /// @return Ok on success or the first non-recoverable error.
        [[nodiscard]] virtual Result<void> runForTicks(std::size_t cycles, std::size_t tickBudget = 0) = 0;

        /// @brief Dispatches a syscall on behalf of a process.
        /// @param pid Calling process identifier.
        /// @param id Syscall identifier.
        /// @param args Syscall arguments.
        /// @return Register-compatible syscall return value.
        [[nodiscard]] virtual Result<RegisterValue>
        syscall(ProcessId pid, SyscallId id, std::span<const RegisterValue> args) = 0;

        /// @brief Registers or replaces syscall handler.
        /// @param id Syscall identifier.
        /// @param handler Handler function.
        /// @return Ok on success or InvalidArgument.
        [[nodiscard]] virtual Result<void> registerSyscallHandler(SyscallId id, SyscallHandlerFn handler) = 0;

        /// @brief Registers a named synchronization primitive.
        /// @param name Primitive name.
        /// @param primitive Primitive instance.
        /// @return Ok on success or AlreadyExists/InvalidArgument.
        [[nodiscard]] virtual Result<void>
        registerSyncPrimitive(const std::string &name, std::unique_ptr<ISyncPrimitive> primitive) = 0;

        /// @brief Acquires a named synchronization primitive.
        [[nodiscard]] virtual Result<void> enterCritical(ProcessId pid, std::string_view primitiveName) = 0;

        /// @brief Releases a named synchronization primitive.
        [[nodiscard]] virtual Result<void> leaveCritical(ProcessId pid, std::string_view primitiveName) = 0;

        /// @brief Returns a snapshot of current kernel state.
        [[nodiscard]] virtual KernelSnapshot snapshot() const = 0;

        /// @brief Returns current simulation time.
        [[nodiscard]] virtual Tick now() const noexcept = 0;

        /// @brief Returns true when process exists in dispatcher.
        [[nodiscard]] virtual bool hasProcess(ProcessId pid) const noexcept = 0;

        /// @brief Returns total managed process count.
        [[nodiscard]] virtual std::size_t processCount() const noexcept = 0;
    };

} // namespace contur
