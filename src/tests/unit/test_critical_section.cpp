/// @file test_critical_section.cpp
/// @brief Unit tests for CriticalSection — verifies it forwards to the underlying
///        ISyncPrimitive, surfaces the inner primitive's layer, and exposes
///        enter/leave/tryEnter aliases consistent with acquire/release/tryAcquire.

#include <memory>

#include <gtest/gtest.h>

#include "contur/core/error.h"
#include "contur/core/types.h"

#include "contur/sync/critical_section.h"
#include "contur/sync/i_sync_primitive.h"
#include "contur/sync/mutex.h"
#include "contur/sync/semaphore.h"

using namespace contur;

namespace {

    constexpr ProcessId PID_A = 1;
    constexpr ProcessId PID_B = 2;

    /// Fake primitive that records every call for forwarding verification.
    class RecorderPrimitive final : public ISyncPrimitive
    {
        public:
        [[nodiscard]] Result<void> acquire(ProcessId pid) override
        {
            ++acquireCalls;
            lastPid = pid;
            return Result<void>::ok();
        }
        [[nodiscard]] Result<void> release(ProcessId pid) override
        {
            ++releaseCalls;
            lastPid = pid;
            return Result<void>::ok();
        }
        [[nodiscard]] Result<void> tryAcquire(ProcessId pid) override
        {
            ++tryAcquireCalls;
            lastPid = pid;
            return Result<void>::ok();
        }
        [[nodiscard]] std::string_view name() const noexcept override
        {
            return "Recorder";
        }
        [[nodiscard]] SyncLayer layer() const noexcept override
        {
            return SyncLayer::KernelInternal;
        }

        int acquireCalls = 0;
        int releaseCalls = 0;
        int tryAcquireCalls = 0;
        ProcessId lastPid = INVALID_PID;
    };

} // namespace

TEST(CriticalSectionTest, DefaultConstructionUsesMutex)
{
    CriticalSection cs;

    EXPECT_EQ(cs.name(), "CriticalSection");
    // Mutex returns SimulatedResource layer.
    EXPECT_EQ(cs.layer(), SyncLayer::SimulatedResource);

    ASSERT_TRUE(cs.acquire(PID_A).isOk());
    ASSERT_TRUE(cs.release(PID_A).isOk());
}

TEST(CriticalSectionTest, AcquireReleaseRoundTripWithMutex)
{
    CriticalSection cs(std::make_unique<Mutex>());

    EXPECT_TRUE(cs.acquire(PID_A).isOk());
    EXPECT_TRUE(cs.release(PID_A).isOk());
}

TEST(CriticalSectionTest, ReleaseByNonOwnerFails)
{
    CriticalSection cs(std::make_unique<Mutex>());

    ASSERT_TRUE(cs.acquire(PID_A).isOk());

    auto badRelease = cs.release(PID_B);
    ASSERT_TRUE(badRelease.isError());
    EXPECT_EQ(badRelease.errorCode(), ErrorCode::PermissionDenied);
}

TEST(CriticalSectionTest, TryAcquireReturnsResourceBusyWhenHeld)
{
    CriticalSection cs(std::make_unique<Mutex>());

    ASSERT_TRUE(cs.acquire(PID_A).isOk());
    auto try2 = cs.tryAcquire(PID_B);
    ASSERT_TRUE(try2.isError());
    EXPECT_EQ(try2.errorCode(), ErrorCode::ResourceBusy);
}

TEST(CriticalSectionTest, EnterLeaveTryEnterAreAliasesForAcquireReleaseTryAcquire)
{
    auto rec = std::make_unique<RecorderPrimitive>();
    auto *recPtr = rec.get();
    CriticalSection cs(std::move(rec));

    ASSERT_TRUE(cs.enter(PID_A).isOk());
    EXPECT_EQ(recPtr->acquireCalls, 1);
    EXPECT_EQ(recPtr->lastPid, PID_A);

    ASSERT_TRUE(cs.leave(PID_A).isOk());
    EXPECT_EQ(recPtr->releaseCalls, 1);

    ASSERT_TRUE(cs.tryEnter(PID_B).isOk());
    EXPECT_EQ(recPtr->tryAcquireCalls, 1);
    EXPECT_EQ(recPtr->lastPid, PID_B);
}

TEST(CriticalSectionTest, LayerForwardsFromInnerPrimitive)
{
    auto rec = std::make_unique<RecorderPrimitive>();
    CriticalSection cs(std::move(rec));

    // Recorder returns KernelInternal, so adapter must reflect that.
    EXPECT_EQ(cs.layer(), SyncLayer::KernelInternal);
}

TEST(CriticalSectionTest, NameIsStableRegardlessOfInner)
{
    CriticalSection csDefault;
    CriticalSection csRec(std::make_unique<RecorderPrimitive>());

    EXPECT_EQ(csDefault.name(), "CriticalSection");
    EXPECT_EQ(csRec.name(), "CriticalSection");
}

TEST(CriticalSectionTest, WorksWithSemaphoreAsInnerPrimitive)
{
    CriticalSection cs(std::make_unique<Semaphore>(1, 1));

    ASSERT_TRUE(cs.acquire(PID_A).isOk());
    // Second acquirer must observe ResourceBusy via tryAcquire path.
    auto try2 = cs.tryAcquire(PID_B);
    ASSERT_TRUE(try2.isError());
    EXPECT_EQ(try2.errorCode(), ErrorCode::ResourceBusy);

    ASSERT_TRUE(cs.release(PID_A).isOk());
    EXPECT_TRUE(cs.tryAcquire(PID_B).isOk());
}

TEST(CriticalSectionTest, MoveConstructionPreservesOwnedState)
{
    CriticalSection original(std::make_unique<Mutex>());
    ASSERT_TRUE(original.acquire(PID_A).isOk());

    CriticalSection moved(std::move(original));

    // The acquired state is held by the inner primitive — moved adapter
    // observes a still-locked mutex.
    auto try2 = moved.tryAcquire(PID_B);
    ASSERT_TRUE(try2.isError());
    EXPECT_EQ(try2.errorCode(), ErrorCode::ResourceBusy);

    EXPECT_TRUE(moved.release(PID_A).isOk());
}

TEST(CriticalSectionTest, ImplementsISyncPrimitiveInterface)
{
    std::unique_ptr<ISyncPrimitive> sync = std::make_unique<CriticalSection>();

    EXPECT_EQ(sync->name(), "CriticalSection");
    ASSERT_TRUE(sync->acquire(PID_A).isOk());
    ASSERT_TRUE(sync->release(PID_A).isOk());
}
