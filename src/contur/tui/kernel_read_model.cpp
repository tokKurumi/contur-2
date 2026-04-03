/// @file kernel_read_model.cpp
/// @brief KernelReadModel implementation.

#include <utility>

#include "contur/kernel/i_kernel_diagnostics.h"
#include "contur/tui/i_kernel_read_model.h"

#include <cstdint>

namespace contur {

    struct KernelReadModel::Impl
    {
        const IKernelDiagnostics &diagnostics;
        mutable std::uint64_t nextSequence = 0;

        explicit Impl(const IKernelDiagnostics &diagnosticsRef)
            : diagnostics(diagnosticsRef)
        {}
    };

    KernelReadModel::KernelReadModel(const IKernelDiagnostics &diagnostics)
        : impl_(std::make_unique<Impl>(diagnostics))
    {}

    KernelReadModel::~KernelReadModel() = default;
    KernelReadModel::KernelReadModel(KernelReadModel &&) noexcept = default;
    KernelReadModel &KernelReadModel::operator=(KernelReadModel &&) noexcept = default;

    Result<TuiSnapshot> KernelReadModel::captureSnapshot() const
    {
        if (!impl_)
        {
            return Result<TuiSnapshot>::error(ErrorCode::InvalidState);
        }

        auto diagnosticsSnapshot = impl_->diagnostics.captureSnapshot();
        if (diagnosticsSnapshot.isError())
        {
            return Result<TuiSnapshot>::error(diagnosticsSnapshot.errorCode());
        }

        const KernelSnapshot &kernelSnapshot = diagnosticsSnapshot.value().kernel;

        TuiSnapshot uiSnapshot;
        uiSnapshot.currentTick = kernelSnapshot.currentTick;
        uiSnapshot.processCount = kernelSnapshot.processCount;
        uiSnapshot.sequence = ++impl_->nextSequence;
        uiSnapshot.epoch = static_cast<std::uint64_t>(kernelSnapshot.currentTick);

        uiSnapshot.scheduler.readyCount = kernelSnapshot.readyCount;
        uiSnapshot.scheduler.blockedCount = kernelSnapshot.blockedCount;
        uiSnapshot.scheduler.runningQueue = kernelSnapshot.runningPids;
        uiSnapshot.scheduler.readyQueue = kernelSnapshot.readyQueue;
        uiSnapshot.scheduler.blockedQueue = kernelSnapshot.blockedQueue;
        uiSnapshot.scheduler.perLaneReadyQueues = kernelSnapshot.perLaneReadyQueues;
        uiSnapshot.scheduler.policyName = kernelSnapshot.policyName;

        uiSnapshot.processes.reserve(kernelSnapshot.processes.size());
        for (const auto &process : kernelSnapshot.processes)
        {
            uiSnapshot.processes.push_back(
                TuiProcessSnapshot{
                    .id = process.id,
                    .name = process.name,
                    .state = process.state,
                    .basePriority = process.basePriority,
                    .effectivePriority = process.effectivePriority,
                    .nice = process.nice,
                    .cpuTime = process.cpuTime,
                    .laneIndex = process.laneIndex,
                }
            );
        }

        uiSnapshot.memory.totalVirtualSlots = kernelSnapshot.totalVirtualSlots;
        uiSnapshot.memory.freeVirtualSlots = kernelSnapshot.freeVirtualSlots;
        uiSnapshot.memory.totalFrames = kernelSnapshot.totalFrames;
        uiSnapshot.memory.freeFrames = kernelSnapshot.freeFrames;
        uiSnapshot.memory.frameOwners = kernelSnapshot.frameOwners;

        return Result<TuiSnapshot>::ok(std::move(uiSnapshot));
    }

} // namespace contur
