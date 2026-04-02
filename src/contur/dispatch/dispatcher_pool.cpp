/// @file dispatcher_pool.cpp
/// @brief Worker-pool dispatch runtime implementation.

#include "contur/dispatch/dispatcher_pool.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "contur/dispatch/i_dispatcher.h"

namespace contur {

    namespace {

        [[nodiscard]] HostThreadingConfig normalizePoolConfig(HostThreadingConfig config)
        {
            HostThreadingConfig normalized = config.normalized();
            if (normalized.hostThreadCount == 0)
            {
                normalized.hostThreadCount = 1;
            }
            if (normalized.isSingleThreaded())
            {
                normalized.workStealingEnabled = false;
            }
            return normalized;
        }

    } // namespace

    struct DispatcherPool::Impl
    {
        enum class JobKind : std::uint8_t
        {
            None,
            Dispatch,
            Tick,
        };

        explicit Impl(HostThreadingConfig runtimeConfig)
            : config(normalizePoolConfig(runtimeConfig))
        {}

        ~Impl()
        {
            stopWorkers();
        }

        Impl(const Impl &) = delete;
        Impl &operator=(const Impl &) = delete;

        [[nodiscard]] bool ensureWorkersStarted()
        {
            if (!workers.empty())
            {
                return true;
            }

            if (startupFailed)
            {
                return false;
            }

            try
            {
                workers.reserve(config.hostThreadCount);
                for (std::size_t workerIndex = 0; workerIndex < config.hostThreadCount; ++workerIndex)
                {
                    workers.emplace_back([this, workerIndex] { workerLoop(workerIndex); });
                }
            }
            catch (...)
            {
                startupFailed = true;
                stopWorkers();
                return false;
            }

            return true;
        }

        void stopWorkers()
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                stopping = true;
                ++jobToken;
            }
            jobReady.notify_all();

            for (std::thread &worker : workers)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }
            workers.clear();
        }

        void beginJob(JobKind nextJob, const DispatcherLanes &nextLanes, std::size_t nextTickBudget)
        {
            std::lock_guard<std::mutex> lock(mutex);
            currentJob = nextJob;
            lanes = &nextLanes;
            tickBudget = nextTickBudget;
            activeWorkers = std::min(workers.size(), nextLanes.size());
            nextLane.store(0, std::memory_order_relaxed);
            laneErrors.assign(nextLanes.size(), ErrorCode::NotFound);
            lastTraceEvents.clear();
            lastDeterministicOrder.clear();
            traceSequence.store(0, std::memory_order_relaxed);
            lastEpoch = epochCounter.fetch_add(1, std::memory_order_relaxed) + 1;
            completedWorkers = 0;
            ++jobToken;
        }

        void waitForJobCompletion()
        {
            std::unique_lock<std::mutex> lock(mutex);
            jobDone.wait(lock, [this] { return completedWorkers == workers.size(); });
            currentJob = JobKind::None;
            lanes = nullptr;
            tickBudget = 0;
            activeWorkers = 0;
        }

        void workerLoop(std::size_t workerIndex)
        {
            std::size_t seenJobToken = 0;

            while (true)
            {
                JobKind job = JobKind::None;
                const DispatcherLanes *jobLanes = nullptr;
                std::size_t jobTickBudget = 0;
                std::size_t workerCount = 0;

                {
                    std::unique_lock<std::mutex> lock(mutex);
                    jobReady.wait(lock, [this, &seenJobToken] { return stopping || jobToken != seenJobToken; });

                    if (stopping)
                    {
                        return;
                    }

                    seenJobToken = jobToken;
                    job = currentJob;
                    jobLanes = lanes;
                    jobTickBudget = tickBudget;
                    workerCount = activeWorkers;
                }

                if (jobLanes && workerIndex < workerCount)
                {
                    if (job == JobKind::Dispatch)
                    {
                        runDispatch(workerIndex, workerCount, *jobLanes, jobTickBudget);
                    }
                    else if (job == JobKind::Tick)
                    {
                        runTick(workerIndex, workerCount, *jobLanes);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(mutex);
                    ++completedWorkers;
                    if (completedWorkers == workers.size())
                    {
                        jobDone.notify_one();
                    }
                }
            }
        }

        void runDispatch(
            std::size_t workerIndex, std::size_t workerCount, const DispatcherLanes &jobLanes, std::size_t jobTickBudget
        )
        {
            if (config.deterministicMode || !config.workStealingEnabled)
            {
                for (std::size_t laneIndex = workerIndex; laneIndex < jobLanes.size(); laneIndex += workerCount)
                {
                    Result<void> laneResult = jobLanes[laneIndex].get().dispatch(jobTickBudget);
                    laneErrors[laneIndex] = laneResult.isOk() ? ErrorCode::Ok : laneResult.errorCode();
                    appendTraceEvent(workerIndex, laneIndex, "dispatch");
                }
                return;
            }

            while (true)
            {
                std::size_t laneIndex = nextLane.fetch_add(1, std::memory_order_relaxed);
                if (laneIndex >= jobLanes.size())
                {
                    break;
                }

                Result<void> laneResult = jobLanes[laneIndex].get().dispatch(jobTickBudget);
                laneErrors[laneIndex] = laneResult.isOk() ? ErrorCode::Ok : laneResult.errorCode();
                appendTraceEvent(workerIndex, laneIndex, "dispatch");
            }
        }

        void runTick(std::size_t workerIndex, std::size_t workerCount, const DispatcherLanes &jobLanes)
        {
            if (config.deterministicMode || !config.workStealingEnabled)
            {
                for (std::size_t laneIndex = workerIndex; laneIndex < jobLanes.size(); laneIndex += workerCount)
                {
                    jobLanes[laneIndex].get().tick();
                    appendTraceEvent(workerIndex, laneIndex, "tick");
                }
                return;
            }

            while (true)
            {
                std::size_t laneIndex = nextLane.fetch_add(1, std::memory_order_relaxed);
                if (laneIndex >= jobLanes.size())
                {
                    break;
                }
                jobLanes[laneIndex].get().tick();
                appendTraceEvent(workerIndex, laneIndex, "tick");
            }
        }

        [[nodiscard]] Result<void> aggregateDispatchErrors() const
        {
            bool anySuccess = false;
            ErrorCode firstError = ErrorCode::Ok;

            for (ErrorCode code : laneErrors)
            {
                if (code == ErrorCode::Ok)
                {
                    anySuccess = true;
                    continue;
                }

                if (firstError == ErrorCode::Ok && code != ErrorCode::NotFound)
                {
                    firstError = code;
                }
            }

            if (anySuccess)
            {
                return Result<void>::ok();
            }
            if (firstError != ErrorCode::Ok)
            {
                return Result<void>::error(firstError);
            }
            return Result<void>::error(ErrorCode::NotFound);
        }

        void beginDeterministicEpoch()
        {
            std::lock_guard<std::mutex> lock(mutex);
            lastTraceEvents.clear();
            lastDeterministicOrder.clear();
            traceSequence.store(0, std::memory_order_relaxed);
            lastEpoch = epochCounter.fetch_add(1, std::memory_order_relaxed) + 1;
        }

        void recordDeterministicLane(std::size_t laneIndex, std::string_view operation)
        {
            std::lock_guard<std::mutex> lock(mutex);
            lastDeterministicOrder.push_back(laneIndex);

            TraceEvent event;
            event.subsystem = "dispatch.runtime";
            event.operation = std::string(operation);
            event.details = "lane=" + std::to_string(laneIndex);
            event.workerId = config.hostThreadCount == 0 ? 0 : laneIndex % config.hostThreadCount;
            event.sequence = traceSequence.fetch_add(1, std::memory_order_relaxed);
            event.epoch = lastEpoch;
            lastTraceEvents.push_back(std::move(event));
        }

        void appendTraceEvent(std::size_t workerIndex, std::size_t laneIndex, std::string_view operation)
        {
            std::lock_guard<std::mutex> lock(mutex);

            TraceEvent event;
            event.subsystem = "dispatch.runtime";
            event.operation = std::string(operation);
            event.details = "lane=" + std::to_string(laneIndex);
            event.workerId = workerIndex;
            event.sequence = traceSequence.fetch_add(1, std::memory_order_relaxed);
            event.epoch = lastEpoch;
            lastTraceEvents.push_back(std::move(event));
        }

        [[nodiscard]] std::uint64_t snapshotLastEpoch() const noexcept
        {
            std::lock_guard<std::mutex> lock(mutex);
            return lastEpoch;
        }

        [[nodiscard]] std::vector<std::size_t> snapshotDeterministicOrder() const
        {
            std::lock_guard<std::mutex> lock(mutex);
            return lastDeterministicOrder;
        }

        [[nodiscard]] std::vector<TraceEvent> snapshotTraceEvents() const
        {
            std::lock_guard<std::mutex> lock(mutex);
            return lastTraceEvents;
        }

        HostThreadingConfig config;
        std::vector<std::thread> workers;

        mutable std::mutex mutex;
        std::condition_variable jobReady;
        std::condition_variable jobDone;

        bool stopping = false;
        bool startupFailed = false;

        std::size_t jobToken = 0;
        std::size_t completedWorkers = 0;
        JobKind currentJob = JobKind::None;
        const DispatcherLanes *lanes = nullptr;
        std::size_t tickBudget = 0;
        std::size_t activeWorkers = 0;

        std::atomic<std::size_t> nextLane{0};
        std::atomic<std::uint64_t> epochCounter{0};
        std::atomic<std::uint64_t> traceSequence{0};
        std::vector<ErrorCode> laneErrors;
        std::uint64_t lastEpoch = 0;
        std::vector<std::size_t> lastDeterministicOrder;
        std::vector<TraceEvent> lastTraceEvents;
    };

    DispatcherPool::DispatcherPool(HostThreadingConfig config)
        : impl_(std::make_unique<Impl>(config))
    {}

    DispatcherPool::~DispatcherPool() = default;
    DispatcherPool::DispatcherPool(DispatcherPool &&) noexcept = default;
    DispatcherPool &DispatcherPool::operator=(DispatcherPool &&) noexcept = default;

    std::string_view DispatcherPool::name() const noexcept
    {
        return "DispatcherPool";
    }

    const HostThreadingConfig &DispatcherPool::config() const noexcept
    {
        return impl_->config;
    }

    Result<void> DispatcherPool::dispatch(const DispatcherLanes &lanes, std::size_t tickBudget)
    {
        if (lanes.empty())
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        if (impl_->config.deterministicMode)
        {
            impl_->beginDeterministicEpoch();
            impl_->laneErrors.assign(lanes.size(), ErrorCode::NotFound);

            for (std::size_t laneIndex = 0; laneIndex < lanes.size(); ++laneIndex)
            {
                Result<void> laneResult = lanes[laneIndex].get().dispatch(tickBudget);
                impl_->laneErrors[laneIndex] = laneResult.isOk() ? ErrorCode::Ok : laneResult.errorCode();
                impl_->recordDeterministicLane(laneIndex, "dispatch");
            }

            return impl_->aggregateDispatchErrors();
        }

        if (!impl_->ensureWorkersStarted())
        {
            return Result<void>::error(ErrorCode::InvalidState);
        }

        impl_->beginJob(Impl::JobKind::Dispatch, lanes, tickBudget);
        impl_->jobReady.notify_all();
        impl_->waitForJobCompletion();

        return impl_->aggregateDispatchErrors();
    }

    void DispatcherPool::tick(const DispatcherLanes &lanes)
    {
        if (lanes.empty())
        {
            return;
        }

        if (impl_->config.deterministicMode)
        {
            impl_->beginDeterministicEpoch();
            for (std::size_t laneIndex = 0; laneIndex < lanes.size(); ++laneIndex)
            {
                lanes[laneIndex].get().tick();
                impl_->recordDeterministicLane(laneIndex, "tick");
            }
            return;
        }

        if (!impl_->ensureWorkersStarted())
        {
            return;
        }

        impl_->beginJob(Impl::JobKind::Tick, lanes, 0);
        impl_->jobReady.notify_all();
        impl_->waitForJobCompletion();
    }

    std::uint64_t DispatcherPool::lastEpoch() const noexcept
    {
        return impl_->snapshotLastEpoch();
    }

    std::vector<std::size_t> DispatcherPool::lastDeterministicOrder() const
    {
        return impl_->snapshotDeterministicOrder();
    }

    std::vector<TraceEvent> DispatcherPool::lastTraceEvents() const
    {
        return impl_->snapshotTraceEvents();
    }

} // namespace contur