/// @file clock.h
/// @brief IClock interface and SimulationClock implementation.
///
/// The simulation clock is a monotonically increasing tick counter
/// injected as a dependency into all subsystems that need timing.
/// No global static state — the clock is owned and passed explicitly.

#pragma once

#include <memory>

#include "contur/core/types.h"

namespace contur {

    /// @brief Abstract clock interface for simulation time.
    ///
    /// All subsystems reference time through this interface, allowing
    /// deterministic testing with mock clocks.
    class IClock
    {
      public:
        virtual ~IClock() = default;

        /// @brief Returns the current simulation tick.
        [[nodiscard]] virtual Tick now() const noexcept = 0;

        /// @brief Advances the simulation clock by one tick.
        virtual void tick() = 0;

        /// @brief Resets the clock to tick 0.
        virtual void reset() = 0;
    };

    /// @brief Concrete simulation clock — a simple monotonic tick counter.
    class SimulationClock final : public IClock
    {
      public:
        SimulationClock();
        ~SimulationClock() override;

        // Non-copyable
        SimulationClock(const SimulationClock &) = delete;
        SimulationClock &operator=(const SimulationClock &) = delete;

        // Movable
        SimulationClock(SimulationClock &&) noexcept;
        SimulationClock &operator=(SimulationClock &&) noexcept;

        [[nodiscard]] Tick now() const noexcept override;
        void tick() override;
        void reset() override;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
