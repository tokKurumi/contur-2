/// @file scheduler.cpp
/// @brief Scheduler implementation.

#include "contur/scheduling/scheduler.h"

#include <algorithm>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

#include "contur/core/clock.h"

#include "contur/process/pcb.h"
#include "contur/process/state.h"
#include "contur/scheduling/i_scheduling_policy.h"
#include "contur/scheduling/statistics.h"

namespace contur {

    struct Scheduler::Impl
    {
        // Protects all mutable state. Callers of public methods hold this lock.
        // Internal helpers that end in _nolock assume the caller already holds it.
        mutable std::mutex mutex_;

        std::unique_ptr<ISchedulingPolicy> policy;
        Statistics statistics;
        std::unordered_map<ProcessId, std::reference_wrapper<PCB>> processes;
        std::unordered_map<ProcessId, std::size_t> processLane;
        std::unordered_map<ProcessId, Tick> runStart;
        std::vector<std::vector<std::reference_wrapper<PCB>>> readyByLane;
        std::vector<std::reference_wrapper<PCB>> blocked;
        std::vector<std::optional<std::reference_wrapper<PCB>>> runningByLane;

        explicit Impl(std::unique_ptr<ISchedulingPolicy> policy)
            : policy(std::move(policy))
            , statistics(0.5)
            , readyByLane(1)
            , runningByLane(1)
        {}

        [[nodiscard]] bool isKnown_nolock(ProcessId pid) const noexcept
        {
            return processes.find(pid) != processes.end();
        }

        [[nodiscard]] static std::vector<SchedulingProcessSnapshot>
        asConst(const std::vector<std::reference_wrapper<PCB>> &queue)
        {
            std::vector<SchedulingProcessSnapshot> out;
            out.reserve(queue.size());
            for (const auto &pcb : queue)
            {
                const PCB &process = pcb.get();
                const auto &timing = process.timing();
                const auto &priority = process.priority();
                out.push_back(
                    SchedulingProcessSnapshot{
                        .pid = process.id(),
                        .arrivalTime = timing.arrivalTime,
                        .lastStateChange = timing.lastStateChange,
                        .estimatedBurst = timing.estimatedBurst,
                        .remainingBurst = timing.remainingBurst,
                        .totalWaitTime = timing.totalWaitTime,
                        .effectivePriority = priority.effective,
                        .nice = priority.nice,
                    }
                );
            }
            return out;
        }

        static void removeFrom(std::vector<std::reference_wrapper<PCB>> &queue, ProcessId pid)
        {
            queue.erase(
                std::remove_if(
                    queue.begin(),
                    queue.end(),
                    [pid](const std::reference_wrapper<PCB> &pcb) { return pcb.get().id() == pid; }
                ),
                queue.end()
            );
        }

        [[nodiscard]] std::optional<std::reference_wrapper<PCB>>
        findIn(std::vector<std::reference_wrapper<PCB>> &queue, ProcessId pid)
        {
            auto it = std::find_if(queue.begin(), queue.end(), [pid](const std::reference_wrapper<PCB> &pcb) {
                return pcb.get().id() == pid;
            });
            if (it == queue.end())
            {
                return std::nullopt;
            }
            return *it;
        }

        void recordRunningBurst_nolock(ProcessId pid, Tick now)
        {
            auto it = runStart.find(pid);
            if (it == runStart.end())
            {
                return;
            }
            Tick started = it->second;
            if (now >= started)
            {
                statistics.recordBurst(pid, now - started);
            }
            runStart.erase(it);
        }

        [[nodiscard]] std::optional<std::size_t> findRunningLane_nolock(ProcessId pid) const noexcept
        {
            for (std::size_t lane = 0; lane < runningByLane.size(); ++lane)
            {
                if (runningByLane[lane].has_value() && runningByLane[lane]->get().id() == pid)
                {
                    return lane;
                }
            }
            return std::nullopt;
        }

        void removeFromAllReady_nolock(ProcessId pid)
        {
            for (auto &queue : readyByLane)
            {
                removeFrom(queue, pid);
            }
        }

        [[nodiscard]] static SchedulingProcessSnapshot toSnapshot(const PCB &process)
        {
            const auto &timing = process.timing();
            const auto &priority = process.priority();
            return SchedulingProcessSnapshot{
                .pid = process.id(),
                .arrivalTime = timing.arrivalTime,
                .lastStateChange = timing.lastStateChange,
                .estimatedBurst = timing.estimatedBurst,
                .remainingBurst = timing.remainingBurst,
                .totalWaitTime = timing.totalWaitTime,
                .effectivePriority = priority.effective,
                .nice = priority.nice,
            };
        }

        Result<ProcessId> selectNextForLane_nolock(std::size_t laneIndex, const IClock &clock)
        {
            if (laneIndex >= readyByLane.size())
            {
                return Result<ProcessId>::error(ErrorCode::InvalidArgument);
            }

            if (!policy)
            {
                return Result<ProcessId>::error(ErrorCode::InvalidState);
            }

            auto &ready = readyByLane[laneIndex];
            auto &running = runningByLane[laneIndex];

            if (ready.empty() && running.has_value())
            {
                return Result<ProcessId>::ok(running->get().id());
            }

            if (ready.empty())
            {
                return Result<ProcessId>::error(ErrorCode::NotFound);
            }

            auto readySnapshot = asConst(ready);
            ProcessId candidateId = policy->selectNext(readySnapshot, clock);
            if (candidateId == INVALID_PID)
            {
                return Result<ProcessId>::error(ErrorCode::InvalidState);
            }

            auto candidateOpt = findIn(ready, candidateId);
            if (!candidateOpt.has_value())
            {
                return Result<ProcessId>::error(ErrorCode::InvalidState);
            }
            PCB &candidate = candidateOpt->get();

            if (running.has_value())
            {
                PCB &runningPcb = running->get();
                if (!policy->shouldPreempt(toSnapshot(runningPcb), toSnapshot(candidate), clock))
                {
                    return Result<ProcessId>::ok(runningPcb.id());
                }

                ProcessId runningPid = runningPcb.id();
                recordRunningBurst_nolock(runningPid, clock.now());
                if (!runningPcb.setState(ProcessState::Ready, clock.now()))
                {
                    return Result<ProcessId>::error(ErrorCode::InvalidState);
                }
                ready.push_back(std::ref(runningPcb));
                processLane[runningPid] = laneIndex;
                running.reset();
            }

            removeFrom(ready, candidateId);

            if (!candidate.setState(ProcessState::Running, clock.now()))
            {
                return Result<ProcessId>::error(ErrorCode::InvalidState);
            }

            running = std::ref(candidate);
            processLane[candidate.id()] = laneIndex;
            runStart[candidate.id()] = clock.now();
            return Result<ProcessId>::ok(candidate.id());
        }

        // Block a specific process by PID regardless of which queue it lives in.
        Result<void> blockProcess_nolock(ProcessId pid, Tick currentTick)
        {
            if (!isKnown_nolock(pid))
            {
                return Result<void>::error(ErrorCode::NotFound);
            }

            // Check if it is currently running on some lane.
            if (auto runningLane = findRunningLane_nolock(pid); runningLane.has_value())
            {
                PCB &running = runningByLane[runningLane.value()]->get();
                recordRunningBurst_nolock(pid, currentTick);
                if (!running.setState(ProcessState::Blocked, currentTick))
                {
                    return Result<void>::error(ErrorCode::InvalidState);
                }
                blocked.push_back(std::ref(running));
                runningByLane[runningLane.value()].reset();
                return Result<void>::ok();
            }

            // Check if it is in a ready queue.
            for (auto &queue : readyByLane)
            {
                auto opt = findIn(queue, pid);
                if (opt.has_value())
                {
                    PCB &pcb = opt->get();
                    if (!pcb.setState(ProcessState::Blocked, currentTick))
                    {
                        return Result<void>::error(ErrorCode::InvalidState);
                    }
                    removeFrom(queue, pid);
                    blocked.push_back(std::ref(pcb));
                    return Result<void>::ok();
                }
            }

            return Result<void>::error(ErrorCode::NotFound);
        }
    };

    Scheduler::Scheduler(std::unique_ptr<ISchedulingPolicy> policy)
        : impl_(std::make_unique<Impl>(std::move(policy)))
    {}

    Scheduler::~Scheduler() = default;
    Scheduler::Scheduler(Scheduler &&) noexcept = default;
    Scheduler &Scheduler::operator=(Scheduler &&) noexcept = default;

    Result<void> Scheduler::enqueue(PCB &pcb, Tick currentTick)
    {
        return enqueueToLane(pcb, 0, currentTick);
    }

    Result<void> Scheduler::enqueueToLane(PCB &pcb, std::size_t laneIndex, Tick currentTick)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        if (laneIndex >= impl_->readyByLane.size())
        {
            return Result<void>::error(ErrorCode::InvalidArgument);
        }

        if (pcb.id() == INVALID_PID)
        {
            return Result<void>::error(ErrorCode::InvalidPid);
        }

        if (!impl_->isKnown_nolock(pcb.id()))
        {
            impl_->processes.emplace(pcb.id(), std::ref(pcb));
        }

        for (auto &queue : impl_->readyByLane)
        {
            if (impl_->findIn(queue, pcb.id()).has_value())
            {
                return Result<void>::ok();
            }
        }

        if (impl_->findRunningLane_nolock(pcb.id()).has_value())
        {
            return Result<void>::ok();
        }

        if (pcb.state() == ProcessState::Running)
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        if (!pcb.setState(ProcessState::Ready, currentTick))
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        if (impl_->statistics.hasPrediction(pcb.id()))
        {
            pcb.timing().estimatedBurst = impl_->statistics.predictedBurst(pcb.id());
        }
        impl_->readyByLane[laneIndex].push_back(std::ref(pcb));
        impl_->processLane[pcb.id()] = laneIndex;
        return Result<void>::ok();
    }

    Result<void> Scheduler::dequeue(ProcessId pid)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        if (!impl_->isKnown_nolock(pid))
        {
            return Result<void>::error(ErrorCode::NotFound);
        }

        for (auto &running : impl_->runningByLane)
        {
            if (running.has_value() && running->get().id() == pid)
            {
                running.reset();
            }
        }

        impl_->removeFromAllReady_nolock(pid);
        Impl::removeFrom(impl_->blocked, pid);
        impl_->processLane.erase(pid);
        impl_->runStart.erase(pid);
        impl_->statistics.clear(pid);
        impl_->processes.erase(pid);

        return Result<void>::ok();
    }

    Result<ProcessId> Scheduler::selectNext(const IClock &clock)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        return impl_->selectNextForLane_nolock(0, clock);
    }

    Result<ProcessId> Scheduler::selectNextForLane(std::size_t laneIndex, const IClock &clock)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        return impl_->selectNextForLane_nolock(laneIndex, clock);
    }

    Result<ProcessId> Scheduler::stealNextForLane(std::size_t thiefLane, const IClock &clock)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        if (thiefLane >= impl_->readyByLane.size())
        {
            return Result<ProcessId>::error(ErrorCode::InvalidArgument);
        }

        // Find the victim lane with the largest ready queue.
        std::optional<std::size_t> victimLane;
        std::size_t victimQueueSize = 0;
        for (std::size_t lane = 0; lane < impl_->readyByLane.size(); ++lane)
        {
            if (lane == thiefLane)
            {
                continue;
            }
            std::size_t queueSize = impl_->readyByLane[lane].size();
            if (queueSize > victimQueueSize)
            {
                victimQueueSize = queueSize;
                victimLane = lane;
            }
        }

        if (!victimLane.has_value() || victimQueueSize == 0)
        {
            return Result<ProcessId>::error(ErrorCode::NotFound);
        }

        // Steal from the FRONT of the victim queue to preserve FIFO ordering.
        auto &victimQueue = impl_->readyByLane[victimLane.value()];
        PCB &stolen = victimQueue.front().get();
        victimQueue.erase(victimQueue.begin());

        impl_->readyByLane[thiefLane].push_back(std::ref(stolen));
        impl_->processLane[stolen.id()] = thiefLane;

        return impl_->selectNextForLane_nolock(thiefLane, clock);
    }

    Result<void> Scheduler::blockRunning(Tick currentTick)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        std::optional<std::size_t> runningLane;
        for (std::size_t lane = 0; lane < impl_->runningByLane.size(); ++lane)
        {
            if (impl_->runningByLane[lane].has_value())
            {
                runningLane = lane;
                break;
            }
        }

        if (!runningLane.has_value())
        {
            return Result<void>::error(ErrorCode::NotFound);
        }

        PCB &running = impl_->runningByLane[runningLane.value()]->get();
        ProcessId pid = running.id();
        impl_->recordRunningBurst_nolock(pid, currentTick);

        if (!running.setState(ProcessState::Blocked, currentTick))
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        impl_->blocked.push_back(std::ref(running));
        impl_->runningByLane[runningLane.value()].reset();
        return Result<void>::ok();
    }

    Result<void> Scheduler::blockProcess(ProcessId pid, Tick currentTick)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        return impl_->blockProcess_nolock(pid, currentTick);
    }

    Result<void> Scheduler::unblock(ProcessId pid, Tick currentTick)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        auto processOpt = impl_->findIn(impl_->blocked, pid);
        if (!processOpt.has_value())
        {
            return Result<void>::error(ErrorCode::NotFound);
        }
        PCB &process = processOpt->get();

        if (!process.setState(ProcessState::Ready, currentTick))
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        Impl::removeFrom(impl_->blocked, pid);
        std::size_t lane = 0;
        if (auto laneIt = impl_->processLane.find(pid); laneIt != impl_->processLane.end())
        {
            lane = laneIt->second;
        }
        if (lane >= impl_->readyByLane.size())
        {
            lane = 0;
        }
        impl_->readyByLane[lane].push_back(std::ref(process));
        impl_->processLane[pid] = lane;
        return Result<void>::ok();
    }

    Result<void> Scheduler::terminate(ProcessId pid, Tick currentTick)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        if (!impl_->isKnown_nolock(pid))
        {
            return Result<void>::error(ErrorCode::NotFound);
        }

        std::optional<std::reference_wrapper<PCB>> process;
        if (auto runningLane = impl_->findRunningLane_nolock(pid); runningLane.has_value())
        {
            process = impl_->runningByLane[runningLane.value()];
            impl_->recordRunningBurst_nolock(pid, currentTick);
            impl_->runningByLane[runningLane.value()].reset();
        }
        else
        {
            process = impl_->processes.find(pid)->second;
            impl_->removeFromAllReady_nolock(pid);
            Impl::removeFrom(impl_->blocked, pid);
        }

        if (process.has_value())
        {
            if (!process->get().setState(ProcessState::Terminated, currentTick))
            {
                return Result<void>::error(ErrorCode::InvalidState);
            }
        }

        impl_->runStart.erase(pid);
        impl_->processLane.erase(pid);
        impl_->statistics.clear(pid);
        impl_->processes.erase(pid);

        return Result<void>::ok();
    }

    std::vector<ProcessId> Scheduler::getQueueSnapshot() const
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        std::vector<ProcessId> out;
        for (const auto &queue : impl_->readyByLane)
        {
            out.reserve(out.size() + queue.size());
            for (const auto &pcb : queue)
            {
                out.push_back(pcb.get().id());
            }
        }
        return out;
    }

    std::vector<std::vector<ProcessId>> Scheduler::getPerLaneQueueSnapshot() const
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        std::vector<std::vector<ProcessId>> snapshot;
        snapshot.resize(impl_->readyByLane.size());

        for (std::size_t lane = 0; lane < impl_->readyByLane.size(); ++lane)
        {
            const auto &queue = impl_->readyByLane[lane];
            auto &laneOut = snapshot[lane];
            laneOut.reserve(queue.size());
            for (const auto &pcb : queue)
            {
                laneOut.push_back(pcb.get().id());
            }
        }

        return snapshot;
    }

    std::vector<ProcessId> Scheduler::getBlockedSnapshot() const
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        std::vector<ProcessId> out;
        out.reserve(impl_->blocked.size());
        for (const auto &pcb : impl_->blocked)
        {
            out.push_back(pcb.get().id());
        }
        return out;
    }

    std::vector<ProcessId> Scheduler::runningProcesses() const
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        std::vector<ProcessId> running;
        running.reserve(impl_->runningByLane.size());
        for (const auto &laneRunning : impl_->runningByLane)
        {
            if (laneRunning.has_value())
            {
                running.push_back(laneRunning->get().id());
            }
        }
        return running;
    }

    Result<void> Scheduler::configureLanes(std::size_t laneCount)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        if (laneCount == 0)
        {
            return Result<void>::error(ErrorCode::InvalidArgument);
        }

        if (!impl_->processes.empty())
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        impl_->readyByLane.assign(laneCount, {});
        impl_->runningByLane.assign(laneCount, std::nullopt);
        impl_->processLane.clear();
        return Result<void>::ok();
    }

    std::size_t Scheduler::laneCount() const noexcept
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        return impl_->readyByLane.size();
    }

    Result<void> Scheduler::setPolicy(std::unique_ptr<ISchedulingPolicy> policy)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);

        if (!policy)
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        impl_->policy = std::move(policy);
        return Result<void>::ok();
    }

    std::string_view Scheduler::policyName() const noexcept
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        if (!impl_->policy)
        {
            return "Unconfigured";
        }
        return impl_->policy->name();
    }

} // namespace contur
