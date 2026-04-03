/// @file tui_models.h
/// @brief Immutable DTO contracts for the external TUI MVC model layer.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "contur/core/types.h"

#include "contur/process/priority.h"
#include "contur/process/state.h"

namespace contur {

    /// @brief Immutable process row used by TUI views.
    struct TuiProcessSnapshot
    {
        /// @brief Process identifier.
        ProcessId id = INVALID_PID;

        /// @brief Human-readable process name.
        std::string name;

        /// @brief Current process lifecycle state.
        ProcessState state = ProcessState::New;

        /// @brief Static/base process priority.
        PriorityLevel basePriority = PriorityLevel::Normal;

        /// @brief Dynamic/effective process priority.
        PriorityLevel effectivePriority = PriorityLevel::Normal;

        /// @brief Nice value used for fine-grained priority adjustment.
        std::int32_t nice = NICE_DEFAULT;

        /// @brief Total CPU time consumed by process.
        Tick cpuTime = 0;

        /// @brief Optional scheduler lane ownership metadata.
        std::optional<std::size_t> laneIndex;
    };

    /// @brief Immutable scheduler-focused snapshot consumed by TUI views.
    struct TuiSchedulerSnapshot
    {
        /// @brief Ordered ready queue PIDs when available.
        std::vector<ProcessId> readyQueue;

        /// @brief Ordered blocked queue PIDs when available.
        std::vector<ProcessId> blockedQueue;

        /// @brief Running process IDs across scheduler lanes.
        std::vector<ProcessId> runningQueue;

        /// @brief Per-lane ready queues when available.
        std::vector<std::vector<ProcessId>> perLaneReadyQueues;

        /// @brief Ready queue size summary.
        std::size_t readyCount = 0;

        /// @brief Blocked queue size summary.
        std::size_t blockedCount = 0;

        /// @brief Active scheduling policy name when available.
        std::string policyName;
    };

    /// @brief Immutable memory-focused snapshot consumed by TUI views.
    struct TuiMemorySnapshot
    {
        /// @brief Total number of virtual memory slots.
        std::size_t totalVirtualSlots = 0;

        /// @brief Number of free virtual memory slots.
        std::size_t freeVirtualSlots = 0;

        /// @brief Optional total number of physical frames.
        std::optional<std::size_t> totalFrames;

        /// @brief Optional number of free physical frames.
        std::optional<std::size_t> freeFrames;

        /// @brief Optional frame ownership map (one entry per frame when provided).
        std::vector<std::optional<ProcessId>> frameOwners;
    };

    /// @brief Top-level immutable model snapshot consumed by TUI controller/views.
    struct TuiSnapshot
    {
        /// @brief Current simulation tick represented by this snapshot.
        Tick currentTick = 0;

        /// @brief Total process count summary.
        std::size_t processCount = 0;

        /// @brief Process rows for process-focused views.
        std::vector<TuiProcessSnapshot> processes;

        /// @brief Scheduler-related state.
        TuiSchedulerSnapshot scheduler;

        /// @brief Memory-related state.
        TuiMemorySnapshot memory;

        /// @brief Monotonic snapshot sequence for deterministic ordering.
        std::uint64_t sequence = 0;

        /// @brief Optional runtime epoch metadata for deterministic mode correlation.
        std::uint64_t epoch = 0;
    };

    /// @brief Single history entry stored by the UI history buffer.
    struct TuiHistoryEntry
    {
        /// @brief History index/sequence.
        std::size_t sequence = 0;

        /// @brief Captured immutable TUI snapshot.
        TuiSnapshot snapshot;
    };

} // namespace contur
