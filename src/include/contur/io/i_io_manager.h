/// @file i_io_manager.h
/// @brief IIoManager interface for unified file/socket I/O.

#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include "contur/core/error.h"
#include "contur/core/types.h"

#include "contur/fs/file_descriptor.h"
#include "contur/io/io_types.h"

namespace contur {

    /// @brief Callback used to wake a blocked process when I/O becomes ready.
    using IoWakeCallback = std::function<void(ProcessId)>;

    /// @brief Kernel I/O manager interface.
    class IIoManager
    {
        public:
        virtual ~IIoManager() = default;

        /// @brief Registers a file resource ID to a filesystem path.
        /// @param resourceId Numeric resource identifier.
        /// @param path Absolute file path.
        /// @param kind File or LAN file resource kind.
        /// @return Ok on success; AlreadyExists/InvalidArgument otherwise.
        [[nodiscard]] virtual Result<void>
        registerFile(RegisterValue resourceId, std::string path, IoResourceKind kind) = 0;

        /// @brief Registers a socket resource ID.
        /// @param resourceId Numeric resource identifier.
        /// @param capacity Maximum queued elements.
        /// @param kind Socket resource kind.
        /// @return Ok on success; AlreadyExists/InvalidArgument otherwise.
        [[nodiscard]] virtual Result<void>
        registerSocket(RegisterValue resourceId, std::size_t capacity, IoResourceKind kind) = 0;

        /// @brief Opens a resource and returns a kernel I/O descriptor.
        /// @param resourceId Numeric resource identifier.
        /// @param kind Resource kind.
        /// @param mode Open mode flags (files only).
        /// @return Descriptor on success; error on failure.
        [[nodiscard]] virtual Result<IoDescriptor>
        open(RegisterValue resourceId, IoResourceKind kind, OpenMode mode) = 0;

        /// @brief Closes a kernel I/O descriptor.
        /// @param fd Descriptor to close.
        /// @return Ok on success; NotFound otherwise.
        [[nodiscard]] virtual Result<void> close(IoDescriptor fd) = 0;

        /// @brief Reads one value from the descriptor.
        /// @param fd Descriptor to read from.
        /// @param pid Requesting process id (for blocking waits).
        /// @return Read value on success; ResourceBusy if the call should block.
        [[nodiscard]] virtual Result<RegisterValue> read(IoDescriptor fd, ProcessId pid) = 0;

        /// @brief Writes one value to the descriptor.
        /// @param fd Descriptor to write to.
        /// @param value Value to write.
        /// @return Ok on success; ResourceBusy if the call should block.
        [[nodiscard]] virtual Result<void> write(IoDescriptor fd, RegisterValue value) = 0;

        /// @brief Sets the wake callback invoked when I/O becomes ready.
        virtual void setWakeCallback(IoWakeCallback callback) = 0;
    };

} // namespace contur
