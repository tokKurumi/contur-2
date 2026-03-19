/// @file scheduler.cpp
/// @brief Scheduler implementation.

#include "contur/scheduling/scheduler.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <stdexcept>
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
        std::unique_ptr<ISchedulingPolicy> policy;
        Statistics statistics;
        std::unordered_map<ProcessId, std::reference_wrapper<PCB>> processes;
        std::unordered_map<ProcessId, Tick> runStart;
        std::vector<std::reference_wrapper<PCB>> ready;
        std::vector<std::reference_wrapper<PCB>> blocked;
        std::optional<std::reference_wrapper<PCB>> running;

        explicit Impl(std::unique_ptr<ISchedulingPolicy> policy)
            : policy(std::move(policy))
            , statistics(0.5)
        {}

        [[nodiscard]] bool isKnown(ProcessId pid) const noexcept
        {
            return processes.find(pid) != processes.end();
        }

        [[nodiscard]] static std::vector<std::reference_wrapper<const PCB>>
        asConst(const std::vector<std::reference_wrapper<PCB>> &queue)
        {
            std::vector<std::reference_wrapper<const PCB>> out;
            out.reserve(queue.size());
            for (const auto &pcb : queue)
            {
                out.push_back(std::cref(pcb.get()));
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

        void recordRunningBurst(ProcessId pid, Tick now)
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
    };

    Scheduler::Scheduler(std::unique_ptr<ISchedulingPolicy> policy)
        : impl_(std::make_unique<Impl>(std::move(policy)))
    {
        if (!impl_->policy)
        {
            throw std::invalid_argument("Scheduler requires a policy");
        }
    }

    Scheduler::~Scheduler() = default;
    Scheduler::Scheduler(Scheduler &&) noexcept = default;
    Scheduler &Scheduler::operator=(Scheduler &&) noexcept = default;

    Result<void> Scheduler::enqueue(PCB &pcb, Tick currentTick)
    {
        if (pcb.id() == INVALID_PID)
        {
            return Result<void>::error(ErrorCode::InvalidPid);
        }

        if (!impl_->isKnown(pcb.id()))
        {
            impl_->processes.emplace(pcb.id(), std::ref(pcb));
        }

        if (impl_->findIn(impl_->ready, pcb.id()).has_value())
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
        impl_->ready.push_back(std::ref(pcb));
        return Result<void>::ok();
    }

    Result<void> Scheduler::dequeue(ProcessId pid)
    {
        if (!impl_->isKnown(pid))
        {
            return Result<void>::error(ErrorCode::NotFound);
        }

        if (impl_->running.has_value() && impl_->running->get().id() == pid)
        {
            impl_->running.reset();
        }

        Impl::removeFrom(impl_->ready, pid);
        Impl::removeFrom(impl_->blocked, pid);
        impl_->runStart.erase(pid);
        impl_->statistics.clear(pid);
        impl_->processes.erase(pid);

        return Result<void>::ok();
    }

    Result<ProcessId> Scheduler::selectNext(const IClock &clock)
    {
        if (impl_->ready.empty() && impl_->running.has_value())
        {
            return Result<ProcessId>::ok(impl_->running->get().id());
        }

        if (impl_->ready.empty())
        {
            return Result<ProcessId>::error(ErrorCode::NotFound);
        }

        auto readyConst = Impl::asConst(impl_->ready);
        ProcessId candidateId = impl_->policy->selectNext(readyConst, clock);
        if (candidateId == INVALID_PID)
        {
            return Result<ProcessId>::error(ErrorCode::InvalidState);
        }

        auto candidateOpt = impl_->findIn(impl_->ready, candidateId);
        if (!candidateOpt.has_value())
        {
            return Result<ProcessId>::error(ErrorCode::InvalidState);
        }
        PCB &candidate = candidateOpt->get();

        if (impl_->running.has_value())
        {
            PCB &running = impl_->running->get();
            if (!impl_->policy->shouldPreempt(running, candidate, clock))
            {
                return Result<ProcessId>::ok(running.id());
            }

            ProcessId runningPid = running.id();
            impl_->recordRunningBurst(runningPid, clock.now());
            if (!running.setState(ProcessState::Ready, clock.now()))
            {
                return Result<ProcessId>::error(ErrorCode::InvalidState);
            }
            impl_->ready.push_back(std::ref(running));
            impl_->running.reset();
        }

        Impl::removeFrom(impl_->ready, candidateId);

        if (!candidate.setState(ProcessState::Running, clock.now()))
        {
            return Result<ProcessId>::error(ErrorCode::InvalidState);
        }

        impl_->running = std::ref(candidate);
        impl_->runStart[candidate.id()] = clock.now();
        return Result<ProcessId>::ok(candidate.id());
    }

    Result<void> Scheduler::blockRunning(Tick currentTick)
    {
        if (!impl_->running.has_value())
        {
            return Result<void>::error(ErrorCode::NotFound);
        }

        PCB &running = impl_->running->get();
        ProcessId pid = running.id();
        impl_->recordRunningBurst(pid, currentTick);

        if (!running.setState(ProcessState::Blocked, currentTick))
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        impl_->blocked.push_back(std::ref(running));
        impl_->running.reset();
        return Result<void>::ok();
    }

    Result<void> Scheduler::unblock(ProcessId pid, Tick currentTick)
    {
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
        impl_->ready.push_back(std::ref(process));
        return Result<void>::ok();
    }

    Result<void> Scheduler::terminate(ProcessId pid, Tick currentTick)
    {
        if (!impl_->isKnown(pid))
        {
            return Result<void>::error(ErrorCode::NotFound);
        }

        std::optional<std::reference_wrapper<PCB>> process;
        if (impl_->running.has_value() && impl_->running->get().id() == pid)
        {
            process = impl_->running;
            impl_->recordRunningBurst(pid, currentTick);
            impl_->running.reset();
        }
        else
        {
            process = impl_->processes.find(pid)->second;
            Impl::removeFrom(impl_->ready, pid);
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
        impl_->statistics.clear(pid);
        impl_->processes.erase(pid);

        return Result<void>::ok();
    }

    std::vector<ProcessId> Scheduler::getQueueSnapshot() const
    {
        std::vector<ProcessId> out;
        out.reserve(impl_->ready.size());
        for (const auto &pcb : impl_->ready)
        {
            out.push_back(pcb.get().id());
        }
        return out;
    }

    std::vector<ProcessId> Scheduler::getBlockedSnapshot() const
    {
        std::vector<ProcessId> out;
        out.reserve(impl_->blocked.size());
        for (const auto &pcb : impl_->blocked)
        {
            out.push_back(pcb.get().id());
        }
        return out;
    }

    ProcessId Scheduler::runningProcess() const noexcept
    {
        if (!impl_->running.has_value())
        {
            return INVALID_PID;
        }
        return impl_->running->get().id();
    }

    Result<void> Scheduler::setPolicy(std::unique_ptr<ISchedulingPolicy> policy)
    {
        if (!policy)
        {
            return Result<void>::error(ErrorCode::InvalidArgument);
        }

        impl_->policy = std::move(policy);
        return Result<void>::ok();
    }

    std::string_view Scheduler::policyName() const noexcept
    {
        return impl_->policy->name();
    }

} // namespace contur
