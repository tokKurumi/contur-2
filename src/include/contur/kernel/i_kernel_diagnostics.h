/// @file i_kernel_diagnostics.h
/// @brief Read-only diagnostics contracts exposed by kernel-adjacent adapters.

#pragma once

#include "contur/core/error.h"

#include "contur/kernel/i_kernel.h"

namespace contur {

    /// @brief Diagnostics snapshot used by external observers.
    ///
    /// This wrapper intentionally decouples external consumers from direct
    /// dependency on kernel internals. At this stage it contains the kernel
    /// runtime-agnostic snapshot as-is and can be extended later.
    struct KernelDiagnosticsSnapshot
    {
        /// @brief Runtime-agnostic kernel snapshot.
        KernelSnapshot kernel;
    };

    /// @brief Read-only diagnostics interface for querying kernel state.
    class IKernelDiagnostics
    {
        public:
        /// @brief Virtual destructor for interface-safe polymorphic cleanup.
        virtual ~IKernelDiagnostics() = default;

        /// @brief Captures a diagnostics snapshot.
        /// @return Diagnostics snapshot or an error.
        [[nodiscard]] virtual Result<KernelDiagnosticsSnapshot> captureSnapshot() const = 0;
    };

} // namespace contur
