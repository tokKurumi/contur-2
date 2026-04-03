/// @file test_resource_contention.cpp
/// @brief Contention tests for guarded shared resources.
///
/// ### ThreadSanitizer (TSAN) annotations
/// All tests in this file spawn real `std::thread` workers and are designed to
/// be run under TSAN (CMake preset `tsan`).  TSAN will flag any data race that
/// is not serialised by a mutex or an acquire/release atomic pair.
///
/// How to run:
/// @code
///   cmake --preset tsan
///   cmake --build --preset tsan
///   ctest --preset tsan --output-on-failure
/// @endcode
///
/// Expected outcome under TSAN: zero data-race reports.  A race report here
/// means the corresponding subsystem is missing a lock or an appropriate
/// memory-order annotation.

#include <array>
#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "contur/core/clock.h"

#include "contur/arch/instruction.h"
#include "contur/io/device_manager.h"
#include "contur/io/network_device.h"
#include "contur/ipc/ipc_manager.h"
#include "contur/ipc/message_queue.h"
#include "contur/memory/fifo_replacement.h"
#include "contur/memory/mmu.h"
#include "contur/memory/page_table.h"
#include "contur/memory/physical_memory.h"
#include "contur/tracing/null_tracer.h"

using namespace contur;

// TSAN: PageTable serialises map/translate/unmap with an internal mutex.
// A race here indicates that mutex is absent or not covering all writers.
TEST(ResourceContentionTest, PageTableConcurrentMapTranslateUnmapIsStable)
{
    PageTable table(8);
    std::atomic<std::size_t> unexpectedErrors{0};

    std::vector<std::thread> workers;
    workers.reserve(8);

    for (std::size_t tid = 0; tid < 8; ++tid)
    {
        workers.emplace_back([&table, &unexpectedErrors, tid] {
            for (std::size_t i = 0; i < 500; ++i)
            {
                std::size_t page = i % 4;
                (void)table.map(page, static_cast<FrameId>(tid + 1));
                auto translated = table.translate(page);
                if (translated.isOk())
                {
                    if (translated.value() == INVALID_FRAME)
                    {
                        unexpectedErrors.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                else if (translated.errorCode() != ErrorCode::SegmentationFault)
                {
                    unexpectedErrors.fetch_add(1, std::memory_order_relaxed);
                }
                (void)table.unmap(page);
            }
        });
    }

    for (auto &worker : workers)
    {
        worker.join();
    }

    EXPECT_LE(table.presentCount(), table.pageCount());
    EXPECT_EQ(unexpectedErrors.load(std::memory_order_relaxed), 0u);
}

// TSAN: MMU read and write paths must both hold the same internal lock.
// A race here indicates a missing read lock or an unguarded frame/page update.
TEST(ResourceContentionTest, MmuConcurrentReadWriteIsSerialized)
{
    SimulationClock clock;
    NullTracer tracer(clock);
    PhysicalMemory memory(128);
    Mmu mmu(memory, std::make_unique<FifoReplacement>(), tracer);

    auto allocated = mmu.allocate(1, 8);
    ASSERT_TRUE(allocated.isOk());

    std::atomic<std::size_t> errors{0};
    std::vector<std::thread> workers;
    workers.reserve(8);

    for (std::size_t tid = 0; tid < 4; ++tid)
    {
        workers.emplace_back([&mmu, &errors, tid] {
            for (std::size_t i = 0; i < 300; ++i)
            {
                MemoryAddress addr = static_cast<MemoryAddress>(i % 8);
                Block block{Instruction::Mov, 0, static_cast<std::int32_t>(tid * 1000 + i), 0};
                auto writeResult = mmu.write(1, addr, block);
                if (writeResult.isError())
                {
                    errors.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (std::size_t tid = 0; tid < 4; ++tid)
    {
        workers.emplace_back([&mmu, &errors] {
            for (std::size_t i = 0; i < 300; ++i)
            {
                MemoryAddress addr = static_cast<MemoryAddress>(i % 8);
                auto readResult = mmu.read(1, addr);
                if (readResult.isError())
                {
                    errors.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto &worker : workers)
    {
        worker.join();
    }

    EXPECT_EQ(errors.load(std::memory_order_relaxed), 0u);
}

// TSAN: DeviceManager must protect its device registry and per-device buffers.
// A race here usually means the per-device queue is accessed without a lock.
TEST(ResourceContentionTest, DeviceManagerSerializesPerDeviceOperations)
{
    DeviceManager manager;
    ASSERT_TRUE(manager.registerDevice(std::make_unique<NetworkDevice>(20000)).isOk());

    constexpr DeviceId kNetworkDeviceId = NetworkDevice::NETWORK_DEVICE_ID;
    std::atomic<std::size_t> writes{0};
    std::vector<std::thread> workers;

    for (std::size_t tid = 0; tid < 8; ++tid)
    {
        workers.emplace_back([&manager, &writes, tid] {
            for (std::size_t i = 0; i < 300; ++i)
            {
                auto result = manager.write(kNetworkDeviceId, static_cast<RegisterValue>(tid * 1000 + i));
                if (result.isOk())
                {
                    writes.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto &worker : workers)
    {
        worker.join();
    }

    std::size_t reads = 0;
    while (true)
    {
        auto value = manager.read(kNetworkDeviceId);
        if (value.isError())
        {
            EXPECT_EQ(value.errorCode(), ErrorCode::BufferEmpty);
            break;
        }
        ++reads;
    }

    EXPECT_EQ(reads, writes.load(std::memory_order_relaxed));
}

// TSAN: IpcManager channel-creation must be idempotent under concurrent callers.
// A race here indicates the channel-registry map is not mutex-protected.
TEST(ResourceContentionTest, IpcManagerConcurrentCreateIsIdempotent)
{
    IpcManager manager;
    std::atomic<std::size_t> failures{0};

    std::vector<std::thread> workers;
    for (int t = 0; t < 8; ++t)
    {
        workers.emplace_back([&manager, &failures] {
            for (int i = 0; i < 200; ++i)
            {
                if (manager.createPipe("shared", 128).isError())
                {
                    failures.fetch_add(1, std::memory_order_relaxed);
                }
                auto channel = manager.getChannel("shared");
                if (channel.isError())
                {
                    failures.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto &worker : workers)
    {
        worker.join();
    }

    EXPECT_TRUE(manager.exists("shared"));
    EXPECT_EQ(manager.channelCount(), 1u);
    EXPECT_EQ(failures.load(std::memory_order_relaxed), 0u);
}

// TSAN: MessageQueue must guard its internal ring/deque with a mutex.
// A race here means write() or read() leaves the size counter unprotected.
TEST(ResourceContentionTest, MessageQueueConcurrentSendReceiveUsesChannelGuard)
{
    MessageQueue queue("q", 4096, false);

    std::atomic<std::size_t> sent{0};
    std::vector<std::thread> producers;
    producers.reserve(4);

    for (std::size_t tid = 0; tid < 4; ++tid)
    {
        producers.emplace_back([&queue, &sent, tid] {
            for (std::size_t i = 0; i < 500; ++i)
            {
                std::array<std::byte, 1> payload{std::byte{static_cast<unsigned char>(tid + i)}};
                auto result = queue.write(payload);
                if (result.isOk())
                {
                    sent.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto &producer : producers)
    {
        producer.join();
    }

    std::size_t received = 0;
    std::array<std::byte, 1> payload{};
    while (true)
    {
        auto result = queue.read(payload);
        if (result.isError())
        {
            EXPECT_EQ(result.errorCode(), ErrorCode::BufferEmpty);
            break;
        }
        ++received;
    }

    EXPECT_EQ(received, sent.load(std::memory_order_relaxed));
}
