/// @file io_manager.h
/// @brief IoManager implementation of the unified I/O layer.

#pragma once

#include <memory>

#include "contur/io/i_io_manager.h"

namespace contur {

    class DeviceManager;
    class IFileSystem;

    /// @brief Unified I/O manager for files and socket-like resources.
    class IoManager final : public IIoManager
    {
        public:
        /// @brief Constructs an I/O manager bound to filesystem and device registry.
        IoManager(IFileSystem &fileSystem, DeviceManager &devices);

        /// @brief Destroys the manager.
        ~IoManager() override;

        IoManager(const IoManager &) = delete;
        IoManager &operator=(const IoManager &) = delete;
        IoManager(IoManager &&) noexcept;
        IoManager &operator=(IoManager &&) noexcept;

        /// @copydoc IIoManager::registerFile
        [[nodiscard]] Result<void>
        registerFile(RegisterValue resourceId, std::string path, IoResourceKind kind) override;

        /// @copydoc IIoManager::registerSocket
        [[nodiscard]] Result<void>
        registerSocket(RegisterValue resourceId, std::size_t capacity, IoResourceKind kind) override;

        /// @copydoc IIoManager::open
        [[nodiscard]] Result<IoDescriptor> open(RegisterValue resourceId, IoResourceKind kind, OpenMode mode) override;

        /// @copydoc IIoManager::close
        [[nodiscard]] Result<void> close(IoDescriptor fd) override;

        /// @copydoc IIoManager::read
        [[nodiscard]] Result<RegisterValue> read(IoDescriptor fd, ProcessId pid) override;

        /// @copydoc IIoManager::write
        [[nodiscard]] Result<void> write(IoDescriptor fd, RegisterValue value) override;

        /// @copydoc IIoManager::setWakeCallback
        void setWakeCallback(IoWakeCallback callback) override;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
