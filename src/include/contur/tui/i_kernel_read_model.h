/// @file i_kernel_read_model.h
/// @brief Read-model adapter interface that maps kernel state to TUI snapshots.

#pragma once

#include <memory>

#include "contur/core/error.h"

#include "contur/kernel/i_kernel_diagnostics.h"
#include "contur/tui/tui_models.h"

namespace contur {

    /// @brief Read-only adapter contract for capturing UI-facing kernel snapshots.
    class IKernelReadModel
    {
        public:
        /// @brief Virtual destructor for interface-safe polymorphic cleanup.
        virtual ~IKernelReadModel() = default;

        /// @brief Captures current kernel state into an immutable TUI snapshot.
        /// @return TUI snapshot or error.
        [[nodiscard]] virtual Result<TuiSnapshot> captureSnapshot() const = 0;
    };

    /// @brief Default implementation that adapts kernel snapshot data into TUI models.
    class KernelReadModel final : public IKernelReadModel
    {
        public:
        /// @brief Constructs read-model bound to diagnostics source.
        /// @param diagnostics Diagnostics source used to capture kernel-facing state.
        explicit KernelReadModel(const IKernelDiagnostics &diagnostics);

        /// @brief Destroys read-model.
        ~KernelReadModel() override;

        /// @brief Copy construction is disabled.
        KernelReadModel(const KernelReadModel &) = delete;

        /// @brief Copy assignment is disabled.
        KernelReadModel &operator=(const KernelReadModel &) = delete;

        /// @brief Move-constructs read-model state.
        KernelReadModel(KernelReadModel &&) noexcept;

        /// @brief Move-assigns read-model state.
        KernelReadModel &operator=(KernelReadModel &&) noexcept;

        /// @copydoc IKernelReadModel::captureSnapshot
        [[nodiscard]] Result<TuiSnapshot> captureSnapshot() const override;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
