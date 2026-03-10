/// @file clock.cpp
/// @brief SimulationClock implementation.

#include "contur/core/clock.h"

namespace contur {

    struct SimulationClock::Impl
    {
        Tick currentTick = 0;
    };

    SimulationClock::SimulationClock()
        : impl_(std::make_unique<Impl>())
    {}

    SimulationClock::~SimulationClock() = default;

    SimulationClock::SimulationClock(SimulationClock &&) noexcept = default;
    SimulationClock &SimulationClock::operator=(SimulationClock &&) noexcept = default;

    Tick SimulationClock::now() const noexcept
    {
        return impl_->currentTick;
    }

    void SimulationClock::tick()
    {
        ++impl_->currentTick;
    }

    void SimulationClock::reset()
    {
        impl_->currentTick = 0;
    }

} // namespace contur
