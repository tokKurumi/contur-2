/// @file kernel_read_model.cpp
/// @brief KernelReadModel implementation.

#include <utility>

#include "contur/kernel/i_kernel_diagnostics.h"
#include "contur/tui/i_kernel_read_model.h"

namespace contur {

    struct KernelReadModel::Impl
    {
        const IKernelDiagnostics &diagnostics;

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

        uiSnapshot.scheduler.readyCount = kernelSnapshot.readyCount;
        uiSnapshot.scheduler.blockedCount = kernelSnapshot.blockedCount;
        uiSnapshot.scheduler.runningQueue = kernelSnapshot.runningPids;

        uiSnapshot.memory.totalVirtualSlots = kernelSnapshot.totalVirtualSlots;
        uiSnapshot.memory.freeVirtualSlots = kernelSnapshot.freeVirtualSlots;

        return Result<TuiSnapshot>::ok(std::move(uiSnapshot));
    }

} // namespace contur
