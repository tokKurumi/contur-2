/// @file event.h
/// @brief Lightweight publish/subscribe event system (Observer pattern).
///
/// Event<Args...> is a type-safe, header-only event dispatcher.
/// Producers call emit(), consumers subscribe with callbacks.
/// Used to decouple kernel subsystems from observers (statistics, tracing, UI).

#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

namespace contur {

    /// @brief A unique identifier for an event subscription, used to unsubscribe.
    using SubscriptionId = std::uint64_t;

    /// @brief Type-safe event dispatcher supporting multiple subscribers.
    ///
    /// @tparam Args The argument types passed to subscribers when the event fires.
    ///
    /// Usage:
    /// @code
    ///   Event<int, std::string> onProcess;
    ///   auto id = onProcess.subscribe([](int pid, const std::string& name) {
    ///       std::cout << "Process " << pid << ": " << name << "\n";
    ///   });
    ///   onProcess.emit(1, "init");
    ///   onProcess.unsubscribe(id);
    /// @endcode
    template <typename... Args>
    class Event
    {
    public:
        /// @brief The callback signature for subscribers.
        using Callback = std::function<void(Args...)>;

        Event() = default;
        ~Event() = default;

        // Non-copyable (subscriptions are identity-bound)
        Event(const Event&) = delete;
        Event& operator=(const Event&) = delete;

        // Movable
        Event(Event&&) noexcept = default;
        Event& operator=(Event&&) noexcept = default;

        /// @brief Registers a callback to be invoked when the event fires.
        /// @param callback The function to call on emit().
        /// @return A SubscriptionId that can be used to unsubscribe later.
        [[nodiscard]] SubscriptionId subscribe(Callback callback)
        {
            auto id = nextId_++;
            subscribers_.push_back({id, std::move(callback)});
            return id;
        }

        /// @brief Removes a previously registered subscription.
        /// @param id The SubscriptionId returned by subscribe().
        /// @return true if the subscription was found and removed, false otherwise.
        bool unsubscribe(SubscriptionId id)
        {
            auto it = std::find_if(subscribers_.begin(), subscribers_.end(),
                                [id](const Subscriber& s) { return s.id == id; });
            if (it != subscribers_.end()) {
                subscribers_.erase(it);
                return true;
            }
            return false;
        }

        /// @brief Fires the event, invoking all registered callbacks with the given arguments.
        /// @param args The arguments to forward to each subscriber.
        void emit(Args... args) const
        {
            for (const auto& sub : subscribers_) {
                sub.callback(args...);
            }
        }

        /// @brief Returns the number of active subscribers.
        [[nodiscard]] std::size_t subscriberCount() const noexcept
        {
            return subscribers_.size();
        }

        /// @brief Removes all subscribers.
        void clear()
        {
            subscribers_.clear();
        }

    private:
        struct Subscriber {
            SubscriptionId id;
            Callback callback;
        };

        std::vector<Subscriber> subscribers_;
        SubscriptionId nextId_ = 1;
    };

} // namespace contur
