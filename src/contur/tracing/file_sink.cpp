/// @file file_sink.cpp
/// @brief File trace sink implementation.

#include "contur/tracing/file_sink.h"

#include <fstream>
#include <mutex>
#include <utility>

namespace contur {

    struct FileSink::Impl
    {
        explicit Impl(std::string path)
            : filePath(std::move(path))
            , stream(filePath, std::ios::out | std::ios::app)
        {}

        std::string filePath;
        std::ofstream stream;
        std::mutex writeMutex;
    };

    FileSink::FileSink(std::string filePath)
        : impl_(std::make_unique<Impl>(std::move(filePath)))
    {}

    FileSink::~FileSink() = default;
    FileSink::FileSink(FileSink &&) noexcept = default;
    FileSink &FileSink::operator=(FileSink &&) noexcept = default;

    void FileSink::write(const TraceEvent &event)
    {
        std::lock_guard<std::mutex> lock(impl_->writeMutex);

        if (!impl_->stream.is_open())
        {
            return;
        }

        impl_->stream << "[T=" << event.timestamp << "][" << traceLevelToString(event.level) << "] ";
        for (std::uint32_t i = 0; i < event.depth; ++i)
        {
            impl_->stream << "  ";
        }

        impl_->stream << event.subsystem << "::" << event.operation;
        if (!event.details.empty())
        {
            impl_->stream << " -> " << event.details;
        }

        impl_->stream << '\n';
    }

} // namespace contur
