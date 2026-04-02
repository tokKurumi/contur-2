/// @file console_sink.cpp
/// @brief Console trace sink implementation.

#include "contur/tracing/console_sink.h"

#include <iostream>
#include <mutex>
#include <utility>

namespace contur {

    struct ConsoleSink::Impl
    {
        std::mutex writeMutex;
    };

    ConsoleSink::ConsoleSink()
        : impl_(std::make_unique<Impl>())
    {}

    ConsoleSink::~ConsoleSink() = default;
    ConsoleSink::ConsoleSink(ConsoleSink &&) noexcept = default;
    ConsoleSink &ConsoleSink::operator=(ConsoleSink &&) noexcept = default;

    void ConsoleSink::write(const TraceEvent &event)
    {
        std::lock_guard<std::mutex> lock(impl_->writeMutex);

        std::cout << "[T=" << event.timestamp << "][" << traceLevelToString(event.level) << "] ";
        for (std::uint32_t i = 0; i < event.depth; ++i)
        {
            std::cout << "  ";
        }

        std::cout << event.subsystem << "::" << event.operation;
        if (!event.details.empty())
        {
            std::cout << " -> " << event.details;
        }

        std::cout << '\n';
    }

} // namespace contur
