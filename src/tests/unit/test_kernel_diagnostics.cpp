/// @file test_kernel_diagnostics.cpp
/// @brief Unit tests for KernelDiagnostics adapter.

#include <memory>
#include <span>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "contur/core/error.h"

#include "contur/kernel/i_kernel.h"
#include "contur/kernel/kernel_diagnostics.h"

using namespace contur;

namespace {

    class FakeKernelForDiagnostics final : public IKernel
    {
        public:
        explicit FakeKernelForDiagnostics(KernelSnapshot snapshot)
            : snapshot_(std::move(snapshot))
        {}

        [[nodiscard]] Result<ProcessId> createProcess(const ProcessConfig &config) override
        {
            (void)config;
            return Result<ProcessId>::error(ErrorCode::NotImplemented);
        }

        [[nodiscard]] Result<void> terminateProcess(ProcessId pid) override
        {
            (void)pid;
            return Result<void>::error(ErrorCode::NotImplemented);
        }

        [[nodiscard]] Result<void> tick(std::size_t tickBudget = 0) override
        {
            (void)tickBudget;
            return Result<void>::error(ErrorCode::NotImplemented);
        }

        [[nodiscard]] Result<void> runForTicks(std::size_t cycles, std::size_t tickBudget = 0) override
        {
            (void)cycles;
            (void)tickBudget;
            return Result<void>::error(ErrorCode::NotImplemented);
        }

        [[nodiscard]] Result<RegisterValue>
        syscall(ProcessId pid, SyscallId id, std::span<const RegisterValue> args) override
        {
            (void)pid;
            (void)id;
            (void)args;
            return Result<RegisterValue>::error(ErrorCode::NotImplemented);
        }

        [[nodiscard]] Result<void> registerSyscallHandler(SyscallId id, SyscallHandlerFn handler) override
        {
            (void)id;
            (void)handler;
            return Result<void>::error(ErrorCode::NotImplemented);
        }

        [[nodiscard]] Result<void>
        registerSyncPrimitive(const std::string &name, std::unique_ptr<ISyncPrimitive> primitive) override
        {
            (void)name;
            (void)primitive;
            return Result<void>::error(ErrorCode::NotImplemented);
        }

        [[nodiscard]] Result<void> enterCritical(ProcessId pid, std::string_view primitiveName) override
        {
            (void)pid;
            (void)primitiveName;
            return Result<void>::error(ErrorCode::NotImplemented);
        }

        [[nodiscard]] Result<void> leaveCritical(ProcessId pid, std::string_view primitiveName) override
        {
            (void)pid;
            (void)primitiveName;
            return Result<void>::error(ErrorCode::NotImplemented);
        }

        [[nodiscard]] KernelSnapshot snapshot() const override
        {
            return snapshot_;
        }

        [[nodiscard]] Tick now() const noexcept override
        {
            return snapshot_.currentTick;
        }

        [[nodiscard]] bool hasProcess(ProcessId pid) const noexcept override
        {
            (void)pid;
            return false;
        }

        [[nodiscard]] std::size_t processCount() const noexcept override
        {
            return snapshot_.processCount;
        }

        private:
        KernelSnapshot snapshot_;
    };

} // namespace

TEST(KernelDiagnosticsTest, CaptureSnapshotReturnsKernelSnapshotAsDiagnosticsPayload)
{
    KernelSnapshot kernelSnapshot;
    kernelSnapshot.currentTick = 77;
    kernelSnapshot.processCount = 4;
    kernelSnapshot.readyCount = 2;
    kernelSnapshot.blockedCount = 1;
    kernelSnapshot.runningPids = {10, 11};
    kernelSnapshot.totalVirtualSlots = 32;
    kernelSnapshot.freeVirtualSlots = 12;

    FakeKernelForDiagnostics kernel(kernelSnapshot);
    KernelDiagnostics diagnostics(kernel);

    auto captured = diagnostics.captureSnapshot();

    ASSERT_TRUE(captured.isOk());

    const KernelDiagnosticsSnapshot &diag = captured.value();
    EXPECT_EQ(diag.kernel.currentTick, 77u);
    EXPECT_EQ(diag.kernel.processCount, 4u);
    EXPECT_EQ(diag.kernel.readyCount, 2u);
    EXPECT_EQ(diag.kernel.blockedCount, 1u);
    EXPECT_EQ(diag.kernel.runningPids, kernelSnapshot.runningPids);
    EXPECT_EQ(diag.kernel.totalVirtualSlots, 32u);
    EXPECT_EQ(diag.kernel.freeVirtualSlots, 12u);
}
