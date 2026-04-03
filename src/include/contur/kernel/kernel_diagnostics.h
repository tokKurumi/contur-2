/// @file kernel_diagnostics.h
/// @brief Default IKernelDiagnostics adapter implementation.

#pragma once

#include <memory>

#include "contur/kernel/i_kernel_diagnostics.h"

namespace contur {

    class IKernel;

    /// @brief Diagnostics adapter that captures snapshots from IKernel facade.
    class KernelDiagnostics final : public IKernelDiagnostics
    {
        public:
        /// @brief Constructs diagnostics adapter bound to a kernel facade.
        /// @param kernel Kernel facade used as diagnostics source.
        explicit KernelDiagnostics(const IKernel &kernel);

        /// @brief Destroys diagnostics adapter.
        ~KernelDiagnostics() override;

        /// @brief Copy construction is disabled.
        KernelDiagnostics(const KernelDiagnostics &) = delete;

        /// @brief Copy assignment is disabled.
        KernelDiagnostics &operator=(const KernelDiagnostics &) = delete;

        /// @brief Move-constructs diagnostics adapter state.
        KernelDiagnostics(KernelDiagnostics &&) noexcept;

        /// @brief Move-assigns diagnostics adapter state.
        KernelDiagnostics &operator=(KernelDiagnostics &&) noexcept;

        /// @copydoc IKernelDiagnostics::captureSnapshot
        [[nodiscard]] Result<KernelDiagnosticsSnapshot> captureSnapshot() const override;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
