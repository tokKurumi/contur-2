/// @file kernel_diagnostics.cpp
/// @brief KernelDiagnostics implementation.

#include "contur/kernel/kernel_diagnostics.h"

#include <memory>
#include <utility>

#include "contur/kernel/i_kernel.h"

namespace contur {

    struct KernelDiagnostics::Impl
    {
        const IKernel &kernel;

        explicit Impl(const IKernel &kernelRef)
            : kernel(kernelRef)
        {}
    };

    KernelDiagnostics::KernelDiagnostics(const IKernel &kernel)
        : impl_(std::make_unique<Impl>(kernel))
    {}

    KernelDiagnostics::~KernelDiagnostics() = default;
    KernelDiagnostics::KernelDiagnostics(KernelDiagnostics &&) noexcept = default;
    KernelDiagnostics &KernelDiagnostics::operator=(KernelDiagnostics &&) noexcept = default;

    Result<KernelDiagnosticsSnapshot> KernelDiagnostics::captureSnapshot() const
    {
        if (!impl_)
        {
            return Result<KernelDiagnosticsSnapshot>::error(ErrorCode::InvalidState);
        }

        KernelDiagnosticsSnapshot snapshot;
        snapshot.kernel = impl_->kernel.snapshot();
        return Result<KernelDiagnosticsSnapshot>::ok(std::move(snapshot));
    }

} // namespace contur
