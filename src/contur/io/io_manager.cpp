/// @file io_manager.cpp
/// @brief IoManager implementation.

#include "contur/io/io_manager.h"

#include <array>
#include <charconv>
#include <deque>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "contur/fs/i_filesystem.h"
#include "contur/io/device_manager.h"
#include "contur/io/network_device.h"

namespace contur {

    namespace {

        struct FileResource
        {
            std::string path;
            IoResourceKind kind = IoResourceKind::File;
        };

        struct SocketStream
        {
            std::size_t capacity = 0;
            std::deque<RegisterValue> buffer;
        };

        struct IoHandle
        {
            IoResourceKind kind = IoResourceKind::File;
            FileDescriptor fileFd{};
            RegisterValue resourceId = 0;
        };

        Result<RegisterValue> parseRegisterValue(std::string_view text)
        {
            RegisterValue value = 0;
            auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            if (result.ec != std::errc())
            {
                return Result<RegisterValue>::error(ErrorCode::InvalidState);
            }
            return Result<RegisterValue>::ok(value);
        }

        Result<RegisterValue> readFileValue(IFileSystem &fs, FileDescriptor fd)
        {
            std::array<std::byte, 64> buffer{};
            auto readResult = fs.read(fd, std::span<std::byte>(buffer.data(), buffer.size()));
            if (readResult.isError())
            {
                return Result<RegisterValue>::error(readResult.errorCode());
            }
            if (readResult.value() == 0)
            {
                return Result<RegisterValue>::error(ErrorCode::EndOfFile);
            }

            std::string text;
            text.reserve(readResult.value());
            for (std::size_t i = 0; i < readResult.value(); ++i)
            {
                text.push_back(static_cast<char>(buffer[i]));
            }
            return parseRegisterValue(text);
        }

        Result<void> writeFileValue(IFileSystem &fs, FileDescriptor fd, RegisterValue value)
        {
            const std::string text = std::to_string(value);
            std::vector<std::byte> bytes(text.size());
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                bytes[i] = static_cast<std::byte>(text[i]);
            }

            auto writeResult = fs.write(fd, std::span<const std::byte>(bytes.data(), bytes.size()));
            if (writeResult.isError())
            {
                return Result<void>::error(writeResult.errorCode());
            }
            return Result<void>::ok();
        }

    } // namespace

    struct IoManager::Impl
    {
        IFileSystem &fileSystem;
        DeviceManager &devices;
        IoWakeCallback wakeCallback;

        std::unordered_map<RegisterValue, FileResource> files;
        std::unordered_map<RegisterValue, SocketStream> sockets;
        std::unordered_map<std::int32_t, IoHandle> handles;
        std::unordered_map<std::int32_t, std::deque<ProcessId>> waiters;
        std::int32_t nextFd = 0;

        Impl(IFileSystem &fileSystem, DeviceManager &devices)
            : fileSystem(fileSystem)
            , devices(devices)
        {}

        Result<void> touchLanDevice(RegisterValue value)
        {
            if (!devices.hasDevice(NetworkDevice::NETWORK_DEVICE_ID))
            {
                return Result<void>::ok();
            }
            auto writeResult = devices.write(NetworkDevice::NETWORK_DEVICE_ID, value);
            if (writeResult.isError())
            {
                return Result<void>::error(writeResult.errorCode());
            }
            auto readResult = devices.read(NetworkDevice::NETWORK_DEVICE_ID);
            if (readResult.isError())
            {
                return Result<void>::error(readResult.errorCode());
            }
            return Result<void>::ok();
        }

        Result<IoHandle> findHandle(IoDescriptor fd)
        {
            auto it = handles.find(fd.value);
            if (it == handles.end())
            {
                return Result<IoHandle>::error(ErrorCode::NotFound);
            }
            return Result<IoHandle>::ok(it->second);
        }
    };

    IoManager::IoManager(IFileSystem &fileSystem, DeviceManager &devices)
        : impl_(std::make_unique<Impl>(fileSystem, devices))
    {}

    IoManager::~IoManager() = default;
    IoManager::IoManager(IoManager &&) noexcept = default;
    IoManager &IoManager::operator=(IoManager &&) noexcept = default;

    Result<void> IoManager::registerFile(RegisterValue resourceId, std::string path, IoResourceKind kind)
    {
        if (resourceId < 0 || path.empty())
        {
            return Result<void>::error(ErrorCode::InvalidArgument);
        }
        if (kind != IoResourceKind::File && kind != IoResourceKind::LanFile)
        {
            return Result<void>::error(ErrorCode::InvalidArgument);
        }
        if (impl_->files.find(resourceId) != impl_->files.end())
        {
            return Result<void>::error(ErrorCode::AlreadyExists);
        }

        impl_->files.emplace(resourceId, FileResource{std::move(path), kind});
        return Result<void>::ok();
    }

    Result<void> IoManager::registerSocket(RegisterValue resourceId, std::size_t capacity, IoResourceKind kind)
    {
        if (resourceId < 0 || capacity == 0)
        {
            return Result<void>::error(ErrorCode::InvalidArgument);
        }
        if (kind != IoResourceKind::SocketStream)
        {
            return Result<void>::error(ErrorCode::NotImplemented);
        }
        if (impl_->sockets.find(resourceId) != impl_->sockets.end())
        {
            return Result<void>::error(ErrorCode::AlreadyExists);
        }

        SocketStream stream;
        stream.capacity = capacity;
        impl_->sockets.emplace(resourceId, std::move(stream));
        return Result<void>::ok();
    }

    Result<IoDescriptor> IoManager::open(RegisterValue resourceId, IoResourceKind kind, OpenMode mode)
    {
        if (resourceId < 0)
        {
            return Result<IoDescriptor>::error(ErrorCode::InvalidArgument);
        }

        IoHandle handle;
        handle.kind = kind;
        handle.resourceId = resourceId;

        if (kind == IoResourceKind::File || kind == IoResourceKind::LanFile)
        {
            auto it = impl_->files.find(resourceId);
            if (it == impl_->files.end())
            {
                return Result<IoDescriptor>::error(ErrorCode::NotFound);
            }
            auto openResult = impl_->fileSystem.open(it->second.path, mode);
            if (openResult.isError())
            {
                return Result<IoDescriptor>::error(openResult.errorCode());
            }
            handle.fileFd = openResult.value();
        }
        else if (kind == IoResourceKind::SocketStream)
        {
            if (impl_->sockets.find(resourceId) == impl_->sockets.end())
            {
                return Result<IoDescriptor>::error(ErrorCode::NotFound);
            }
        }
        else
        {
            return Result<IoDescriptor>::error(ErrorCode::NotImplemented);
        }

        IoDescriptor fd{impl_->nextFd++};
        impl_->handles.emplace(fd.value, handle);
        return Result<IoDescriptor>::ok(fd);
    }

    Result<void> IoManager::close(IoDescriptor fd)
    {
        auto handleResult = impl_->findHandle(fd);
        if (handleResult.isError())
        {
            return Result<void>::error(handleResult.errorCode());
        }

        const IoHandle &handle = handleResult.value();
        if (handle.kind == IoResourceKind::File || handle.kind == IoResourceKind::LanFile)
        {
            auto closeResult = impl_->fileSystem.close(handle.fileFd);
            if (closeResult.isError())
            {
                return Result<void>::error(closeResult.errorCode());
            }
        }

        impl_->handles.erase(fd.value);
        impl_->waiters.erase(fd.value);
        return Result<void>::ok();
    }

    Result<RegisterValue> IoManager::read(IoDescriptor fd, ProcessId pid)
    {
        auto handleResult = impl_->findHandle(fd);
        if (handleResult.isError())
        {
            return Result<RegisterValue>::error(handleResult.errorCode());
        }
        const IoHandle &handle = handleResult.value();

        if (handle.kind == IoResourceKind::File || handle.kind == IoResourceKind::LanFile)
        {
            auto valueResult = readFileValue(impl_->fileSystem, handle.fileFd);
            if (valueResult.isError())
            {
                return Result<RegisterValue>::error(valueResult.errorCode());
            }
            if (handle.kind == IoResourceKind::LanFile)
            {
                auto touchResult = impl_->touchLanDevice(valueResult.value());
                if (touchResult.isError())
                {
                    return Result<RegisterValue>::error(touchResult.errorCode());
                }
            }
            return valueResult;
        }

        if (handle.kind == IoResourceKind::SocketStream)
        {
            auto socketIt = impl_->sockets.find(handle.resourceId);
            if (socketIt == impl_->sockets.end())
            {
                return Result<RegisterValue>::error(ErrorCode::NotFound);
            }
            SocketStream &stream = socketIt->second;
            if (stream.buffer.empty())
            {
                impl_->waiters[fd.value].push_back(pid);
                return Result<RegisterValue>::error(ErrorCode::ResourceBusy);
            }
            RegisterValue value = stream.buffer.front();
            stream.buffer.pop_front();
            return Result<RegisterValue>::ok(value);
        }

        return Result<RegisterValue>::error(ErrorCode::NotImplemented);
    }

    Result<void> IoManager::write(IoDescriptor fd, RegisterValue value)
    {
        auto handleResult = impl_->findHandle(fd);
        if (handleResult.isError())
        {
            return Result<void>::error(handleResult.errorCode());
        }
        const IoHandle &handle = handleResult.value();

        if (handle.kind == IoResourceKind::File || handle.kind == IoResourceKind::LanFile)
        {
            auto writeResult = writeFileValue(impl_->fileSystem, handle.fileFd, value);
            if (writeResult.isError())
            {
                return Result<void>::error(writeResult.errorCode());
            }
            if (handle.kind == IoResourceKind::LanFile)
            {
                auto touchResult = impl_->touchLanDevice(value);
                if (touchResult.isError())
                {
                    return Result<void>::error(touchResult.errorCode());
                }
            }
            return Result<void>::ok();
        }

        if (handle.kind == IoResourceKind::SocketStream)
        {
            auto socketIt = impl_->sockets.find(handle.resourceId);
            if (socketIt == impl_->sockets.end())
            {
                return Result<void>::error(ErrorCode::NotFound);
            }
            SocketStream &stream = socketIt->second;
            if (stream.buffer.size() >= stream.capacity)
            {
                return Result<void>::error(ErrorCode::ResourceBusy);
            }
            stream.buffer.push_back(value);

            auto waitersIt = impl_->waiters.find(fd.value);
            if (waitersIt != impl_->waiters.end() && !waitersIt->second.empty())
            {
                ProcessId pid = waitersIt->second.front();
                waitersIt->second.pop_front();
                if (impl_->wakeCallback)
                {
                    impl_->wakeCallback(pid);
                }
            }
            return Result<void>::ok();
        }

        return Result<void>::error(ErrorCode::NotImplemented);
    }

    void IoManager::setWakeCallback(IoWakeCallback callback)
    {
        impl_->wakeCallback = std::move(callback);
    }

} // namespace contur
