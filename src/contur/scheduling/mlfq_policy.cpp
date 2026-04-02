/// @file mlfq_policy.cpp
/// @brief MLFQ scheduling policy implementation.

#include "contur/scheduling/mlfq_policy.h"

#include <algorithm>

#include "contur/core/clock.h"

namespace contur {

    namespace {
        [[nodiscard]] std::size_t
        priorityToLevel(const SchedulingProcessSnapshot &process, std::size_t levelCount) noexcept
        {
            std::size_t level = static_cast<std::size_t>(process.effectivePriority);
            if (levelCount == 0)
            {
                return 0;
            }
            if (level >= levelCount)
            {
                return levelCount - 1;
            }
            return level;
        }
    } // namespace

    MlfqPolicy::MlfqPolicy(std::vector<std::size_t> levelTimeSlices)
        : levelTimeSlices_(std::move(levelTimeSlices))
    {
        if (levelTimeSlices_.empty())
        {
            levelTimeSlices_.push_back(1);
        }
        for (std::size_t &value : levelTimeSlices_)
        {
            if (value == 0)
            {
                value = 1;
            }
        }
    }

    std::string_view MlfqPolicy::name() const noexcept
    {
        return "MLFQ";
    }

    ProcessId
    MlfqPolicy::selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const
    {
        (void)clock;
        if (readyQueue.empty())
        {
            return INVALID_PID;
        }

        auto selected = std::min_element(readyQueue.begin(), readyQueue.end(), [this](const auto &a, const auto &b) {
            std::size_t levelA = priorityToLevel(a, levelTimeSlices_.size());
            std::size_t levelB = priorityToLevel(b, levelTimeSlices_.size());

            if (levelA != levelB)
            {
                return levelA < levelB;
            }

            if (a.lastStateChange != b.lastStateChange)
            {
                return a.lastStateChange < b.lastStateChange;
            }

            return a.pid < b.pid;
        });

        return selected->pid;
    }

    bool MlfqPolicy::shouldPreempt(
        const SchedulingProcessSnapshot &running, const SchedulingProcessSnapshot &candidate, const IClock &clock
    ) const
    {
        std::size_t runningLevel = priorityToLevel(running, levelTimeSlices_.size());
        std::size_t candidateLevel = priorityToLevel(candidate, levelTimeSlices_.size());

        if (candidateLevel < runningLevel)
        {
            return true;
        }

        Tick elapsed = clock.now() - running.lastStateChange;
        return elapsed >= levelTimeSlices_[runningLevel];
    }

    const std::vector<std::size_t> &MlfqPolicy::levelTimeSlices() const noexcept
    {
        return levelTimeSlices_;
    }

} // namespace contur
