/// @file deadlock_detector.cpp
/// @brief DeadlockDetector implementation.

#include "contur/sync/deadlock_detector.h"

#include <algorithm>
#include <functional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace contur {

    struct DeadlockDetector::Impl
    {
        struct WaitNode
        {
            ProcessId pid = INVALID_PID;
            ThreadToken threadToken = 0;

            [[nodiscard]] bool operator==(const WaitNode &other) const noexcept
            {
                return pid == other.pid && threadToken == other.threadToken;
            }
        };

        struct WaitNodeHash
        {
            [[nodiscard]] std::size_t operator()(const WaitNode &node) const noexcept
            {
                return std::hash<ProcessId>{}(node.pid) ^ (std::hash<ThreadToken>{}(node.threadToken) << 1U);
            }
        };

        std::unordered_map<ResourceId, std::unordered_set<WaitNode, WaitNodeHash>> resourceOwners;
        std::unordered_map<WaitNode, std::unordered_set<WaitNode, WaitNodeHash>, WaitNodeHash> waitFor;

        std::unordered_map<ThreadToken, std::vector<ResourceId>> heldInternalLocks;
        std::unordered_map<ResourceId, std::unordered_set<ResourceId>> lockOrderGraph;

        void clearOutgoing(const WaitNode &node)
        {
            waitFor.erase(node);
        }

        [[nodiscard]] bool hasWaitForCycle() const
        {
            std::unordered_set<WaitNode, WaitNodeHash> visited;
            std::unordered_set<WaitNode, WaitNodeHash> onStack;

            std::function<bool(const WaitNode &)> dfs = [&](const WaitNode &node) {
                if (onStack.find(node) != onStack.end())
                {
                    return true;
                }
                if (visited.find(node) != visited.end())
                {
                    return false;
                }

                visited.insert(node);
                onStack.insert(node);

                auto it = waitFor.find(node);
                if (it != waitFor.end())
                {
                    for (const WaitNode &next : it->second)
                    {
                        if (dfs(next))
                        {
                            return true;
                        }
                    }
                }

                onStack.erase(node);
                return false;
            };

            for (const auto &[node, _] : waitFor)
            {
                if (dfs(node))
                {
                    return true;
                }
            }

            return false;
        }

        [[nodiscard]] std::vector<WaitNode> waitForCycleNodes() const
        {
            std::set<std::pair<ProcessId, ThreadToken>> cycleSet;
            std::unordered_set<WaitNode, WaitNodeHash> visited;
            std::vector<WaitNode> stack;

            std::function<void(const WaitNode &)> dfs = [&](const WaitNode &node) {
                visited.insert(node);
                stack.push_back(node);

                auto it = waitFor.find(node);
                if (it != waitFor.end())
                {
                    for (const WaitNode &next : it->second)
                    {
                        auto pos = std::find(stack.begin(), stack.end(), next);
                        if (pos != stack.end())
                        {
                            for (auto cyc = pos; cyc != stack.end(); ++cyc)
                            {
                                cycleSet.emplace(cyc->pid, cyc->threadToken);
                            }
                        }
                        else if (visited.find(next) == visited.end())
                        {
                            dfs(next);
                        }
                    }
                }

                stack.pop_back();
            };

            for (const auto &[node, _] : waitFor)
            {
                if (visited.find(node) == visited.end())
                {
                    dfs(node);
                }
            }

            std::vector<WaitNode> nodes;
            nodes.reserve(cycleSet.size());
            for (const auto &[pid, threadToken] : cycleSet)
            {
                nodes.push_back(WaitNode{.pid = pid, .threadToken = threadToken});
            }
            return nodes;
        }

        [[nodiscard]] bool hasLockOrderCycle() const
        {
            std::unordered_set<ResourceId> visited;
            std::unordered_set<ResourceId> onStack;

            std::function<bool(ResourceId)> dfs = [&](ResourceId lockId) {
                if (onStack.find(lockId) != onStack.end())
                {
                    return true;
                }
                if (visited.find(lockId) != visited.end())
                {
                    return false;
                }

                visited.insert(lockId);
                onStack.insert(lockId);

                auto it = lockOrderGraph.find(lockId);
                if (it != lockOrderGraph.end())
                {
                    for (ResourceId next : it->second)
                    {
                        if (dfs(next))
                        {
                            return true;
                        }
                    }
                }

                onStack.erase(lockId);
                return false;
            };

            for (const auto &[lockId, _] : lockOrderGraph)
            {
                if (dfs(lockId))
                {
                    return true;
                }
            }

            return false;
        }

        [[nodiscard]] std::vector<ResourceId> lockOrderCycleNodes() const
        {
            std::set<ResourceId> cycleSet;
            std::unordered_set<ResourceId> visited;
            std::vector<ResourceId> stack;

            std::function<void(ResourceId)> dfs = [&](ResourceId lockId) {
                visited.insert(lockId);
                stack.push_back(lockId);

                auto it = lockOrderGraph.find(lockId);
                if (it != lockOrderGraph.end())
                {
                    for (ResourceId next : it->second)
                    {
                        auto pos = std::find(stack.begin(), stack.end(), next);
                        if (pos != stack.end())
                        {
                            for (auto cyc = pos; cyc != stack.end(); ++cyc)
                            {
                                cycleSet.insert(*cyc);
                            }
                        }
                        else if (visited.find(next) == visited.end())
                        {
                            dfs(next);
                        }
                    }
                }

                stack.pop_back();
            };

            for (const auto &[lockId, _] : lockOrderGraph)
            {
                if (visited.find(lockId) == visited.end())
                {
                    dfs(lockId);
                }
            }

            return std::vector<ResourceId>(cycleSet.begin(), cycleSet.end());
        }
    };

    DeadlockDetector::DeadlockDetector()
        : impl_(std::make_unique<Impl>())
    {}

    DeadlockDetector::~DeadlockDetector() = default;
    DeadlockDetector::DeadlockDetector(DeadlockDetector &&) noexcept = default;
    DeadlockDetector &DeadlockDetector::operator=(DeadlockDetector &&) noexcept = default;

    void DeadlockDetector::onAcquire(ProcessId pid, ResourceId resource)
    {
        onAcquire(pid, resource, 0);
    }

    void DeadlockDetector::onAcquire(ProcessId pid, ResourceId resource, ThreadToken threadToken)
    {
        if (pid == INVALID_PID)
        {
            return;
        }

        Impl::WaitNode node{.pid = pid, .threadToken = threadToken};
        impl_->resourceOwners[resource].insert(node);
        impl_->clearOutgoing(node);
    }

    void DeadlockDetector::onRelease(ProcessId pid, ResourceId resource)
    {
        onRelease(pid, resource, 0);
    }

    void DeadlockDetector::onRelease(ProcessId pid, ResourceId resource, ThreadToken threadToken)
    {
        auto ownerIt = impl_->resourceOwners.find(resource);
        if (ownerIt == impl_->resourceOwners.end())
        {
            return;
        }

        ownerIt->second.erase(Impl::WaitNode{.pid = pid, .threadToken = threadToken});
        if (ownerIt->second.empty())
        {
            impl_->resourceOwners.erase(ownerIt);
        }
    }

    void DeadlockDetector::onWait(ProcessId pid, ResourceId resource)
    {
        onWait(pid, resource, 0);
    }

    void DeadlockDetector::onWait(ProcessId pid, ResourceId resource, ThreadToken threadToken)
    {
        if (pid == INVALID_PID)
        {
            return;
        }

        Impl::WaitNode waitingNode{.pid = pid, .threadToken = threadToken};
        impl_->clearOutgoing(waitingNode);

        auto ownerIt = impl_->resourceOwners.find(resource);
        if (ownerIt == impl_->resourceOwners.end())
        {
            return;
        }

        for (const Impl::WaitNode &owner : ownerIt->second)
        {
            if (!(owner == waitingNode))
            {
                impl_->waitFor[waitingNode].insert(owner);
            }
        }
    }

    bool DeadlockDetector::hasDeadlock() const
    {
        return impl_->hasWaitForCycle() || impl_->hasLockOrderCycle();
    }

    std::vector<ProcessId> DeadlockDetector::getDeadlockedProcesses() const
    {
        std::set<ProcessId> processSet;
        for (const auto &node : impl_->waitForCycleNodes())
        {
            processSet.insert(node.pid);
        }
        return std::vector<ProcessId>(processSet.begin(), processSet.end());
    }

    void DeadlockDetector::onInternalLockAcquire(ThreadToken threadToken, ResourceId lockId)
    {
        auto &held = impl_->heldInternalLocks[threadToken];
        for (ResourceId heldLock : held)
        {
            if (heldLock != lockId)
            {
                impl_->lockOrderGraph[heldLock].insert(lockId);
            }
        }
        held.push_back(lockId);
    }

    void DeadlockDetector::onInternalLockRelease(ThreadToken threadToken, ResourceId lockId)
    {
        auto heldIt = impl_->heldInternalLocks.find(threadToken);
        if (heldIt == impl_->heldInternalLocks.end())
        {
            return;
        }

        auto &held = heldIt->second;
        auto lockIt = std::find(held.begin(), held.end(), lockId);
        if (lockIt != held.end())
        {
            held.erase(lockIt);
        }

        if (held.empty())
        {
            impl_->heldInternalLocks.erase(heldIt);
        }
    }

    bool DeadlockDetector::hasInternalLockOrderCycle() const
    {
        return impl_->hasLockOrderCycle();
    }

    std::vector<ResourceId> DeadlockDetector::getInternalLockOrderCycle() const
    {
        return impl_->lockOrderCycleNodes();
    }

    bool DeadlockDetector::isSafeState(
        const std::vector<ResourceAllocation> &current,
        const std::vector<ResourceAllocation> &maximum,
        const std::vector<std::uint32_t> &available
    ) const
    {
        if (current.size() != maximum.size())
        {
            return false;
        }
        if (current.empty())
        {
            return true;
        }

        const std::size_t processCount = current.size();
        const std::size_t resourceCount = available.size();
        if (resourceCount == 0)
        {
            return true;
        }

        std::unordered_map<ProcessId, std::size_t> indexByPid;
        indexByPid.reserve(processCount);

        for (std::size_t i = 0; i < processCount; ++i)
        {
            if (current[i].pid == INVALID_PID || maximum[i].pid == INVALID_PID)
            {
                return false;
            }
            if (current[i].pid != maximum[i].pid)
            {
                return false;
            }
            if (current[i].resources.size() != resourceCount || maximum[i].resources.size() != resourceCount)
            {
                return false;
            }
            if (indexByPid.find(current[i].pid) != indexByPid.end())
            {
                return false;
            }
            indexByPid[current[i].pid] = i;
        }

        for (std::size_t i = 0; i < processCount; ++i)
        {
            for (std::size_t r = 0; r < resourceCount; ++r)
            {
                if (current[i].resources[r] > maximum[i].resources[r])
                {
                    return false;
                }
            }
        }

        std::vector<std::uint32_t> work = available;

        std::vector<bool> finished(processCount, false);
        std::size_t finishCount = 0;

        while (true)
        {
            bool progressed = false;
            for (std::size_t i = 0; i < processCount; ++i)
            {
                if (finished[i])
                {
                    continue;
                }

                bool canFinish = true;
                for (std::size_t r = 0; r < resourceCount; ++r)
                {
                    std::uint32_t need = maximum[i].resources[r] - current[i].resources[r];
                    if (need > work[r])
                    {
                        canFinish = false;
                        break;
                    }
                }

                if (canFinish)
                {
                    for (std::size_t r = 0; r < resourceCount; ++r)
                    {
                        work[r] += current[i].resources[r];
                    }
                    finished[i] = true;
                    ++finishCount;
                    progressed = true;
                }
            }

            if (!progressed)
            {
                break;
            }
        }

        return finishCount == processCount;
    }

} // namespace contur
