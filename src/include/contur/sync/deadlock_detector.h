/// @file deadlock_detector.h
/// @brief Deadlock detection and prevention utilities.

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "contur/core/types.h"

namespace contur {

    using ResourceId = std::uint32_t;

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
        DeadlockDetector();
        ~DeadlockDetector();

        DeadlockDetector(const DeadlockDetector &) = delete;
        DeadlockDetector &operator=(const DeadlockDetector &) = delete;
        DeadlockDetector(DeadlockDetector &&) noexcept;
        DeadlockDetector &operator=(DeadlockDetector &&) noexcept;

        /// @brief Records successful acquisition of a resource by a process.
        /// @param pid Process that acquired resource.
        /// @param resource Acquired resource identifier.
        void onAcquire(ProcessId pid, ResourceId resource);

        /// @brief Records release of a resource by a process.
        /// @param pid Process that released resource.
        /// @param resource Released resource identifier.
        void onRelease(ProcessId pid, ResourceId resource);

        /// @brief Records waiting on a resource and updates wait-for graph.
        /// @param pid Waiting process.
        /// @param resource Requested resource identifier.
        void onWait(ProcessId pid, ResourceId resource);

        /// @brief Returns true if the current wait-for graph has a cycle.
        [[nodiscard]] bool hasDeadlock() const;

        /// @brief Returns process IDs involved in deadlock cycles.
        [[nodiscard]] std::vector<ProcessId> getDeadlockedProcesses() const;

        /// @brief Banker's algorithm safety check.
        ///
        /// `current[i].resources[r]` is currently allocated count for process i, resource r.
        /// `maximum[i].resources[r]` is maximum demand for process i, resource r.
        /// `available[r]` is currently available count for resource r.
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
