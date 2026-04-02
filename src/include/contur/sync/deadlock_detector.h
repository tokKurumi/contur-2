/// @file deadlock_detector.h
/// @brief Deadlock detection and prevention utilities.

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "contur/core/types.h"

namespace contur {

    /// @brief Host-thread token used by thread-aware deadlock tracking.
    using ThreadToken = std::uint64_t;

    /// @brief Per-process resource vector used by Banker's safety check.
    struct ResourceAllocation
    {
        /// @brief Process identifier for this allocation row.
        ProcessId pid = INVALID_PID;
        /// @brief Resource vector indexed by resource type.
        std::vector<std::uint32_t> resources;
    };

    /// @brief Deadlock detection/prevention helper.
    ///
    /// Maintains a wait-for graph for cycle detection and provides
    /// Banker's safety check for allocation state validation.
    class DeadlockDetector
    {
        public:
        /// @brief Constructs deadlock detector with empty graphs.
        DeadlockDetector();

        /// @brief Destroys deadlock detector.
        ~DeadlockDetector();

        /// @brief Copy construction is disabled.
        DeadlockDetector(const DeadlockDetector &) = delete;

        /// @brief Copy assignment is disabled.
        DeadlockDetector &operator=(const DeadlockDetector &) = delete;
        /// @brief Move-constructs detector state.
        DeadlockDetector(DeadlockDetector &&) noexcept;

        /// @brief Move-assigns detector state.
        DeadlockDetector &operator=(DeadlockDetector &&) noexcept;

        /// @brief Records successful acquisition of a resource by a process.
        /// @param pid Process that acquired resource.
        /// @param resource Acquired resource identifier.
        void onAcquire(ProcessId pid, ResourceId resource);

        /// @brief Thread-aware variant of resource acquisition tracking.
        void onAcquire(ProcessId pid, ResourceId resource, ThreadToken threadToken);

        /// @brief Records release of a resource by a process.
        /// @param pid Process that released resource.
        /// @param resource Released resource identifier.
        void onRelease(ProcessId pid, ResourceId resource);

        /// @brief Thread-aware variant of resource release tracking.
        void onRelease(ProcessId pid, ResourceId resource, ThreadToken threadToken);

        /// @brief Records waiting on a resource and updates wait-for graph.
        /// @param pid Waiting process.
        /// @param resource Requested resource identifier.
        void onWait(ProcessId pid, ResourceId resource);

        /// @brief Thread-aware variant of wait edge tracking.
        void onWait(ProcessId pid, ResourceId resource, ThreadToken threadToken);

        /// @brief Returns true if the current wait-for graph has a cycle.
        [[nodiscard]] bool hasDeadlock() const;

        /// @brief Returns process IDs involved in deadlock cycles.
        [[nodiscard]] std::vector<ProcessId> getDeadlockedProcesses() const;

        /// @brief Records internal lock acquisition order for lock-order analysis.
        void onInternalLockAcquire(ThreadToken threadToken, ResourceId lockId);

        /// @brief Records internal lock release for lock-order analysis.
        void onInternalLockRelease(ThreadToken threadToken, ResourceId lockId);

        /// @brief Returns true when internal lock-order graph contains a cycle.
        [[nodiscard]] bool hasInternalLockOrderCycle() const;

        /// @brief Returns lock IDs involved in lock-order cycles.
        [[nodiscard]] std::vector<ResourceId> getInternalLockOrderCycle() const;

        /// @brief Banker's algorithm safety check.
        ///
        /// `current[i].resources[r]` is currently allocated count for process i, resource r.
        /// `maximum[i].resources[r]` is maximum demand for process i, resource r.
        /// `available[r]` is currently available count for resource r.
        /// @param current Currently allocated resources per process.
        /// @param maximum Maximum claims per process.
        /// @param available Currently available resource vector.
        /// @return True when the state is safe; false otherwise.
        [[nodiscard]] bool isSafeState(
            const std::vector<ResourceAllocation> &current,
            const std::vector<ResourceAllocation> &maximum,
            const std::vector<std::uint32_t> &available
        ) const;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
