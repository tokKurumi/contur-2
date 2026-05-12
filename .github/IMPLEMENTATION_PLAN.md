# Contur 2 — Implementation Plan

> This document is the step-by-step execution plan for building the Contur 2 OS Kernel Simulator from scratch.
> Each phase lists concrete tasks, files to create, acceptance criteria, and dependencies.
> Refer to `contur2.instructions.md` for architecture, interfaces, and code style rules.

**Status legend:** ✅ complete · 🔄 partially complete (implementation present but requirements not fully met)

---

## Phase 0: Project Scaffolding

**Goal**: Set up the directory structure, build system, and tooling so that `cmake --preset debug` produces an empty but compiling executable.

### Tasks

| # | Task | Files | Done |
|---|---|---|---|
| 0.1 | Create `src/` directory tree (all subdirs from instructions) | `src/include/contur/{core,arch,memory,process,execution,cpu,scheduling,dispatch,sync,ipc,syscall,fs,io,tui,tracing,kernel}/` + `src/contur/...` + `src/app/` + `src/demos/` + `src/tests/` | ✅ |
| 0.2 | Root `CMakeLists.txt` with C++20, Clang, Ninja | `src/CMakeLists.txt` | ✅ |
| 0.3 | `CMakePresets.json` (debug, release, gcc-debug) | `src/CMakePresets.json` | ✅ |
| 0.4 | Library CMake target `contur2_lib` (STATIC, empty for now) | `src/CMakeLists.txt` (lib section) | ✅ |
| 0.5 | App CMake target `contur2` linking `contur2_lib` | `src/app/CMakeLists.txt` | ✅ |
| 0.6 | Stub `main.cpp` that compiles and prints "Contur 2" | `src/app/main.cpp` | ✅ |
| 0.7 | Demos CMake target `contur2_demos` | `src/demos/CMakeLists.txt` | ✅ |
| 0.8 | Tests CMake with Google Test `FetchContent` | `src/tests/CMakeLists.txt` | ✅ |
| 0.9 | Build script wrapper | `src/build.sh` | ✅ |
| 0.10 | `.clang-format` at project root | `.clang-format` | ✅ |
| 0.11 | `.clang-tidy` at project root | `.clang-tidy` | ✅ |
| 0.12 | Verify: `cmake --preset debug -S src && cmake --build --preset debug` succeeds | — | ✅ |

### Acceptance Criteria
- `./src/build/debug/app/contur2` runs and exits cleanly
- `ctest --preset debug` runs (0 tests, 0 failures)
- `clang-format --dry-run` reports no issues on all files

---

## Phase 1: Foundation (`core/` + `arch/`)

**Goal**: Establish the type system, error handling, simulation clock, event system, and architecture-level enums/structures that all other modules depend on.

**Dependencies**: Phase 0

### Tasks — `core/`

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 1.1 | `types.h` — type aliases (`ProcessId`, `MemoryAddress`, `Tick`, `RegisterValue`, `DeviceId`) + sentinel constants | `core/types.h` | — (header-only) | — | ✅ |
| 1.2 | `error.h` — `ErrorCode` enum class + `Result<T>` template (ok/error factory, `isOk()`, `value()`, `errorCode()`) + `Result<void>` specialization | `core/error.h` | — (header-only template) | `test_result.cpp` | ✅ |
| 1.3 | `clock.h` — `IClock` interface + `SimulationClock` (PIMPL) | `core/clock.h` | `core/clock.cpp` | `test_clock.cpp` | ✅ |
| 1.4 | `event.h` — `Event<Args...>` template (subscribe, unsubscribe, emit) | `core/event.h` | — (header-only template) | `test_event.cpp` | ✅ |

### Tasks — `arch/`

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 1.5 | `instruction.h` — `Instruction` enum class (Nop, Mov, Add, Sub, Mul, Div, And, Or, Xor, Shift, Cmp, Jxx, Push, Pop, Call, Ret, Rmem, Wmem, Int, Halt) | `arch/instruction.h` | — | — | ✅ |
| 1.6 | `interrupt.h` — `Interrupt` enum class (Ok, Error, Exit, SystemCall, DivByZero, DeviceIO, NetworkIO, Timer, PageFault) | `arch/interrupt.h` | — | — | ✅ |
| 1.7 | `block.h` — `Block` struct (instruction code, register index, operand, state flag) | `arch/block.h` | — | — | ✅ |
| 1.8 | `register_file.h` — `Register` enum class + `RegisterFile` (std::array<RegisterValue, 16>, get/set, reset, debug print) | `arch/register_file.h` | `arch/register_file.cpp` | `test_register_file.cpp` | ✅ |
| 1.9 | `isa.h` — ISA constants and helpers (register count, instruction name strings, `constexpr` decode helpers) | `arch/isa.h` | — | — | ✅ |

### Acceptance Criteria
- All `core/` and `arch/` types compile with zero warnings
- `Result<T>` handles ok and error paths in tests
- `SimulationClock` ticks and reports correct time
- `Event<>` subscribe/emit/unsubscribe works correctly
- `RegisterFile` read/write/reset passes tests

---

## Phase 2: Memory Subsystem (`memory/`)

**Goal**: Implement physical memory, MMU with address translation and swap, page table, and page replacement policies.

**Dependencies**: Phase 1

### Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 2.1 | `i_memory.h` — `IMemory` interface (read, write, size, clear) | `memory/i_memory.h` | — | — | ✅ |
| 2.2 | `physical_memory.h` — `PhysicalMemory` (PIMPL; `std::vector<Block>` backing store) | `memory/physical_memory.h` | `memory/physical_memory.cpp` | `test_physical_memory.cpp` | ✅ |
| 2.3 | `page_table.h` — `PageTable` (virtual→physical frame mapping, present/dirty/reference bits) | `memory/page_table.h` | `memory/page_table.cpp` | `test_page_table.cpp` | ✅ |
| 2.4 | `i_page_replacement.h` — `IPageReplacementPolicy` interface (selectVictim, onAccess, onLoad) | `memory/i_page_replacement.h` | — | — | ✅ |
| 2.5 | Page replacement implementations: `FifoReplacement`, `LruReplacement`, `ClockReplacement`, `OptimalReplacement` | `memory/fifo_replacement.h` + 3 more | `memory/fifo_replacement.cpp` + 3 | `test_page_replacement.cpp` | ✅ |
| 2.6 | `i_mmu.h` — `IMMU` interface (swapIn, swapOut, translate) | `memory/i_mmu.h` | — | — | ✅ |
| 2.7 | `mmu.h` — `Mmu` (PIMPL; uses IMemory, PageTable, IPageReplacementPolicy) | `memory/mmu.h` | `memory/mmu.cpp` | `test_mmu.cpp` | ✅ |
| 2.8 | `i_virtual_memory.h` + `virtual_memory.h` — virtual address space manager (allocate/free slots for ProcessImage) | `memory/i_virtual_memory.h`, `memory/virtual_memory.h` | `memory/virtual_memory.cpp` | `test_virtual_memory.cpp` | ✅ |

### Acceptance Criteria
- PhysicalMemory: write then read returns correct Block
- PageTable: map/unmap/translate works; present/dirty bits tracked
- All 4 page replacement policies select correct victim in controlled scenarios
- MMU: swapIn loads code to real address, swapOut clears it, translate converts virtual→real

---

## Phase 3: Process Model (`process/`)

**Goal**: Build the process representation with priorities and validated state transitions — using composition, not inheritance.

**Dependencies**: Phase 1

### Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 3.1 | `state.h` — `ProcessState` enum class + `isValidTransition()` constexpr function | `process/state.h` | — | `test_process_state.cpp` | ✅ |
| 3.2 | `priority.h` — `PriorityLevel` enum class (7 levels), `Priority` struct (base, effective, nice) | `process/priority.h` | — | `test_priority.cpp` | ✅ |
| 3.3 | `pcb.h` — `PCB` (PIMPL; id, name, state, priority, timing, address info) | `process/pcb.h` | `process/pcb.cpp` | `test_pcb.cpp` | ✅ |
| 3.4 | `process_image.h` — `ProcessImage` (contains PCB + `std::vector<Block>` code + `RegisterFile`; PIMPL) | `process/process_image.h` | `process/process_image.cpp` | `test_process_image.cpp` | ✅ |
| 3.5 | `i_process.h` — `IProcess` read-only interface for external consumers | `process/i_process.h` | — | — | ✅ |

### Acceptance Criteria
- State transitions: valid ones succeed, invalid ones return error/throw
- Priority: base/effective/nice independently modifiable
- PCB: full round-trip of all fields
- ProcessImage: contains code, register file, PCB; all accessible

---

## Phase 4: CPU + I/O (`cpu/` + `io/`)

**Goal**: Implement the fetch-decode-execute cycle with ALU, and the I/O device abstraction.

**Dependencies**: Phase 1, Phase 2 (IMemory), Phase 3 (ProcessImage for register context)

### Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 4.1 | `alu.h` — `ALU` (pure arithmetic/logic operations: add, sub, mul, div, and, or, xor, shift, cmp; returns Result) | `cpu/alu.h` | `cpu/alu.cpp` | `test_alu.cpp` | ✅ |
| 4.2 | `i_cpu.h` — `ICPU` interface (step, reset) | `cpu/i_cpu.h` | — | — | ✅ |
| 4.3 | `cpu.h` — `Cpu` (PIMPL; uses IMemory&, ALU, RegisterFile; fetch-decode-execute) | `cpu/cpu.h` | `cpu/cpu.cpp` | `test_cpu.cpp` | ✅ |
| 4.4 | `i_device.h` — `IDevice` interface (id, name, read, write, isReady) | `io/i_device.h` | — | — | ✅ |
| 4.5 | `console_device.h` — `ConsoleDevice` (PIMPL; stdout output, echo buffer for reads) | `io/console_device.h` | `io/console_device.cpp` | `test_device_manager.cpp` (ConsoleDeviceTest) | ✅ |
| 4.6 | `network_device.h` — `NetworkDevice` (PIMPL; deque buffer with capacity, FIFO, `BufferFull`/`BufferEmpty` errors) | `io/network_device.h` | `io/network_device.cpp` | `test_device_manager.cpp` (NetworkDeviceTest) | ✅ |
| 4.7 | `device_manager.h` — `DeviceManager` (registry of `unique_ptr<IDevice>`, dispatch by DeviceId) | `io/device_manager.h` | `io/device_manager.cpp` | `test_device_manager.cpp` | ✅ |

### Acceptance Criteria
- ALU: all arithmetic ops produce correct results; division by zero returns `ErrorCode::DivisionByZero`
- CPU: execute a sequence of Mov/Add/Sub instructions, verify register state
- CPU: `Interrupt::Exit` causes clean halt
- DeviceManager: register device, dispatch write, verify output

---

## Phase 5: Interpreter Engine (`execution/`)

**Goal**: Create the bytecode interpreter execution engine behind the `IExecutionEngine` interface.

**Dependencies**: Phase 4 (CPU)

### Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 5.1 | `i_execution_engine.h` — `IExecutionEngine` interface (execute, halt, name) | `execution/i_execution_engine.h` | — | — | ✅ |
| 5.2 | `execution_context.h` — `ExecutionContext` / `ExecutionResult` structs | `execution/execution_context.h` | — | — | ✅ |
| 5.3 | `interpreter_engine.h` — `InterpreterEngine` (PIMPL; wraps CPU; executes block-by-block up to tickBudget) | `execution/interpreter_engine.h` | `execution/interpreter_engine.cpp` | `test_interpreter_engine.cpp` | ✅ |

### Acceptance Criteria
- Load a simple program (Mov, Add, Int Exit) into PhysicalMemory
- InterpreterEngine executes it within tick budget
- Correct register state after execution
- Halt stops execution mid-program

---

## Phase 6: Scheduling (`scheduling/`)

**Goal**: Implement the scheduler with 7 pluggable scheduling policies + statistics/prediction.

**Dependencies**: Phase 3 (PCB, Priority), Phase 1 (IClock)

### Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 6.1 | `i_scheduling_policy.h` — `ISchedulingPolicy` interface (name, selectNext, shouldPreempt) | `scheduling/i_scheduling_policy.h` | — | — | ✅ |
| 6.2 | `fcfs_policy.h` — First Come First Served | `scheduling/fcfs_policy.h` | `scheduling/fcfs_policy.cpp` | `test_fcfs.cpp` | ✅ |
| 6.3 | `round_robin_policy.h` — Round Robin (configurable time slice) | `scheduling/round_robin_policy.h` | `scheduling/round_robin_policy.cpp` | `test_round_robin.cpp` | ✅ |
| 6.4 | `spn_policy.h` — Shortest Process Next | `scheduling/spn_policy.h` | `scheduling/spn_policy.cpp` | `test_spn.cpp` | ✅ |
| 6.5 | `srt_policy.h` — Shortest Remaining Time | `scheduling/srt_policy.h` | `scheduling/srt_policy.cpp` | `test_srt.cpp` | ✅ |
| 6.6 | `hrrn_policy.h` — Highest Response Ratio Next | `scheduling/hrrn_policy.h` | `scheduling/hrrn_policy.cpp` | `test_hrrn.cpp` | ✅ |
| 6.7 | `priority_policy.h` — Dynamic Priority scheduling | `scheduling/priority_policy.h` | `scheduling/priority_policy.cpp` | `test_priority_policy.cpp` | ✅ |
| 6.8 | `mlfq_policy.h` — Multilevel Feedback Queue | `scheduling/mlfq_policy.h` | `scheduling/mlfq_policy.cpp` | `test_mlfq.cpp` | ✅ |
| 6.9 | `statistics.h` — `Statistics` (per-process execution history, exponential weighted average prediction) | `scheduling/statistics.h` | `scheduling/statistics.cpp` | `test_statistics.cpp` | ✅ |
| 6.10 | `i_scheduler.h` — `IScheduler` interface (enqueue, dequeue, selectNext, getQueueSnapshot) | `scheduling/i_scheduler.h` | — | — | ✅ |
| 6.11 | `scheduler.h` — `Scheduler` (PIMPL; hosts ISchedulingPolicy, manages state queues, uses Statistics) | `scheduling/scheduler.h` | `scheduling/scheduler.cpp` | `test_scheduler.cpp` | ✅ |

### Acceptance Criteria
- Each policy, given a synthetic ready queue, selects the correct process
- Each policy's `shouldPreempt()` returns correct decision
- Scheduler correctly moves processes between state queues
- Statistics produces predictions matching expected exponential average
- MLFQ cascades processes between levels on time slice expiry

---

## Phase 7: Dispatch + Synchronization (`dispatch/` + `sync/`)

**Goal**: Orchestrate the full process lifecycle and implement synchronization primitives with deadlock detection.

**Dependencies**: Phase 2 (MMU, VirtualMemory), Phase 5 (IExecutionEngine), Phase 6 (Scheduler)

### Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 7.1 | `i_sync_primitive.h` — `ISyncPrimitive` interface (acquire, release, tryAcquire, name) | `sync/i_sync_primitive.h` | — | — | ✅ |
| 7.2 | `mutex.h` — `Mutex` (PIMPL; binary lock with owner tracking) | `sync/mutex.h` | `sync/mutex.cpp` | `test_mutex.cpp` | ✅ |
| 7.3 | `semaphore.h` — `Semaphore` (PIMPL; counting semaphore) | `sync/semaphore.h` | `sync/semaphore.cpp` | `test_semaphore.cpp` | ✅ |
| 7.4 | `critical_section.h` — `CriticalSection` (ISyncPrimitive composition) | `sync/critical_section.h` | `sync/critical_section.cpp` | — | ✅ |
| 7.5 | `deadlock_detector.h` — `DeadlockDetector` (wait-for graph + cycle detection DFS + banker's algorithm) | `sync/deadlock_detector.h` | `sync/deadlock_detector.cpp` | `test_deadlock_detector.cpp` | ✅ |
| 7.6 | `i_dispatcher.h` — `IDispatcher` interface (createProcess, dispatch, tick) | `dispatch/i_dispatcher.h` | — | — | ✅ |
| 7.7 | `dispatcher.h` — `Dispatcher` (PIMPL; lifecycle orchestration: allocate VM → enqueue → schedule → execute → terminate) | `dispatch/dispatcher.h` | `dispatch/dispatcher.cpp` | `test_dispatcher.cpp` | ✅ |
| 7.8 | `mp_dispatcher.h` — `MPDispatcher` (extends Dispatcher for N processors) | `dispatch/mp_dispatcher.h` | `dispatch/mp_dispatcher.cpp` | `test_mp_dispatcher.cpp` | ✅ |

### Acceptance Criteria
- Mutex: acquire/release works; double-acquire by same process is reentrant or returns error
- Semaphore: counting semantics correct (count-down to 0 blocks)
- DeadlockDetector: detects cycle in a 3-process circular wait; banker's algorithm rejects unsafe state
- Dispatcher: process goes through full lifecycle (New → Ready → Running → Terminated)
- Dispatcher: context switch works (registers saved/restored)
- MPDispatcher: distributes processes across N schedulers

---

## Phase 8: IPC & System Calls (`ipc/` + `syscall/`)

**Goal**: Implement inter-process communication channels and a formalized syscall dispatch layer.

**Dependencies**: Phase 7 (Dispatcher for process context), Phase 3 (ProcessImage)

### Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 8.1 | `i_ipc_channel.h` — `IIpcChannel` interface (write, read, close, isOpen, name) | `ipc/i_ipc_channel.h` | — | — | ✅ |
| 8.2 | `pipe.h` — `Pipe` (PIMPL; bounded circular buffer, blocking semantics) | `ipc/pipe.h` | `ipc/pipe.cpp` | `test_pipe.cpp` | ✅ |
| 8.3 | `shared_memory.h` — `SharedMemory` (named region, multi-process attach/detach) | `ipc/shared_memory.h` | `ipc/shared_memory.cpp` | `test_shared_memory.cpp` | ✅ |
| 8.4 | `message_queue.h` — `MessageQueue` (typed FIFO, optional priority) | `ipc/message_queue.h` | `ipc/message_queue.cpp` | `test_message_queue.cpp` | ✅ |
| 8.5 | `ipc_manager.h` — `IpcManager` (registry: create/lookup/destroy channels by name) | `ipc/ipc_manager.h` | `ipc/ipc_manager.cpp` | `test_ipc_manager.cpp` | ✅ |
| 8.6 | `syscall_ids.h` — `SyscallId` enum class (Exit, Read, Write, Fork, Exec, Wait, Open, Close, Pipe, Msg, Shm, Mutex, Sem, GetPid, GetTime, Yield) | `syscall/syscall_ids.h` | — | — | ✅ |
| 8.7 | `syscall_handler.h` — `ISyscallHandler` interface | `syscall/syscall_handler.h` | — | — | ✅ |
| 8.8 | `syscall_table.h` — `SyscallTable` (maps SyscallId → handler; dispatch method) | `syscall/syscall_table.h` | `syscall/syscall_table.cpp` | `test_syscall_table.cpp` | ✅ |

### Acceptance Criteria
- Pipe: write N bytes, read N bytes back in order; blocking when buffer full
- SharedMemory: two processes write/read same region
- MessageQueue: FIFO ordering; priority ordering when enabled
- IpcManager: create by name, lookup, double-create returns existing
- SyscallTable: dispatch to correct handler; unknown syscall returns error

---

## Phase 9: File System (`fs/`)

**Goal**: Implement a minimal inode-based file system simulation.

**Dependencies**: Phase 1 (types, error)

### Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 9.1 | `inode.h` — `Inode` struct (type, size, block pointers, timestamps) | `fs/inode.h` | — | — | ✅ |
| 9.2 | `directory_entry.h` — `DirectoryEntry` struct (name → inode number) | `fs/directory_entry.h` | — | — | ✅ |
| 9.3 | `block_allocator.h` — `BlockAllocator` (bitmap-based, allocate/free/isFree) | `fs/block_allocator.h` | `fs/block_allocator.cpp` | `test_block_allocator.cpp` | ✅ |
| 9.4 | `file_descriptor.h` — `FileDescriptor` + per-process FD table | `fs/file_descriptor.h` | `fs/file_descriptor.cpp` | — | ✅ |
| 9.5 | `i_filesystem.h` — `IFileSystem` interface (open, read, write, close, mkdir, remove, listDir, stat) | `fs/i_filesystem.h` | — | — | ✅ |
| 9.6 | `simple_fs.h` — `SimpleFS` (PIMPL; inode table + block allocator + directory tree over simulated disk) | `fs/simple_fs.h` | `fs/simple_fs.cpp` | `test_simple_fs.cpp` | ✅ |

### Acceptance Criteria
- BlockAllocator: allocate/free blocks; bitmap integrity
- SimpleFS: create file, write data, close, reopen, read same data back
- SimpleFS: mkdir, create file inside, listDir returns it
- SimpleFS: remove file frees blocks and inode
- SimpleFS: stat returns correct size and type

---

## Phase 10: Kernel (`kernel/`)

**Goal**: Compose all subsystems behind a single `IKernel` facade with a builder for dependency injection.

**Dependencies**: Phases 2–9

### Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 10.1 | `i_kernel.h` — `IKernel` interface (createProcess, terminateProcess, syscall, enterCS, leaveCS, tick, snapshot) | `kernel/i_kernel.h` | — | — | ✅ |
| 10.2 | `kernel.h` — `Kernel` (PIMPL; delegates to Dispatcher, IpcManager, SyscallTable, FileSystem) | `kernel/kernel.h` | `kernel/kernel.cpp` | — | ✅ |
| 10.3 | `kernel_builder.h` — `KernelBuilder` (fluent API: withMemory, withCpu, withPolicy, withEngine, withTracer, withDevices, withFileSystem, build) | `kernel/kernel_builder.h` | `kernel/kernel_builder.cpp` | `test_kernel_builder.cpp` | ✅ |
| 10.4 | Integration test: create kernel, create process, run to completion | — | — | `test_kernel_api.cpp` | ✅ |

### Acceptance Criteria
- KernelBuilder assembles a valid Kernel from all components
- Kernel.createProcess + tick() loop runs a simple program to Exit
- Kernel.terminateProcess cleans up correctly
- syscall dispatch through Kernel works end-to-end

---

## Phase 11: Host Multithreading Runtime (`dispatch/` + `scheduling/` + `sync/` + `kernel/`)

**Goal**: Introduce real host-thread parallelism with configurable `N >= 1` while preserving the N=1 baseline path and
preventing architecture drift/spaghetti.

**Dependencies**: Phase 6, Phase 7, Phase 8, Phase 10

### Implementation Blueprint (from-scratch rollout)

This blueprint defines the exact implementation path for host multithreading from scratch.
Follow steps in order; do not skip test gates.

#### Hard Constraints (from architectural review)

- No exception-based control flow in production runtime paths (`throw` is forbidden in scheduler/dispatcher/kernel flow).
- Dependency inversion must be preserved: runtime strategy is injected from composition root, never implicitly created inside
	`MPDispatcher`.
- Missing policy/dependency must surface as explicit `Result<...>::error(ErrorCode::InvalidState)`.
- Runtime injection into `MPDispatcher` is mandatory; absence of runtime is a composition wiring error.
- Kernel must stay runtime-agnostic: no host-thread numeric knobs in `KernelDependencies` or `KernelSnapshot`.
- `threading_config` belongs to runtime/dispatcher components and is consumed internally there.
- Deterministic mode support for `N > 1` is mandatory.
- Deadlock model must cover both simulated resource graph and internal lock-order graph.

#### File-by-file Implementation Sequence

| Step | Files | What to add/fix | Test gate |
|---|---|---|---|
| I1 | `src/include/contur/dispatch/threading_config.h` | Implement `HostThreadingConfig` as runtime-owned config (`hostThreadCount`, `deterministicMode`, `workStealingEnabled`) with normalization helpers (`isValid()`, `isSingleThreaded()`). | Runtime-config unit checks via dispatcher/runtime tests |
| I2 | `src/include/contur/dispatch/i_dispatch_runtime.h`, `src/include/contur/dispatch/serial_dispatch_runtime.h`, `src/contur/dispatch/serial_dispatch_runtime.cpp` | Implement lane runtime strategy abstraction and serial baseline runtime (pluggable only, no hidden fallback usage). | `test_mp_dispatcher.cpp` (fake runtime + serial runtime behavior) |
| I3 | `src/include/contur/dispatch/mp_dispatcher.h`, `src/contur/dispatch/mp_dispatcher.cpp` | Keep runtime injection explicit and mandatory. `MPDispatcher` is constructed only with injected runtime; no unconfigured-runtime mode and no implicit `SerialDispatchRuntime` fallback. | `MPDispatcherTest.EmptyLanesPropagateInvalidStateWithConfiguredRuntime`, custom-runtime tests |
| I4 | `src/include/contur/kernel/kernel_builder.h`, `src/contur/kernel/kernel_builder.cpp` | Composition-root wiring only: inject runtime component/factory into dispatcher assembly. Do not persist runtime numeric config inside `Kernel` state. | `test_kernel_builder.cpp` wiring tests + `test_kernel_api.cpp` error propagation checks |
| I5 | `src/include/contur/kernel/i_kernel.h`, `src/contur/kernel/kernel.cpp` | Keep snapshot focused on kernel state and scheduler/dispatcher views; do not add host-thread numeric config fields into kernel snapshot. | Snapshot compatibility assertions |
| I6 | `src/contur/scheduling/scheduler.cpp` (+ existing scheduler tests) | Ensure null policy never throws; `selectNext()` returns `InvalidState`; `policyName()` reports unconfigured state. | `SchedulerTest.NullPolicyReturnsInvalidStateInsteadOfThrow` |
| I7 | `src/include/contur/dispatch/dispatcher_pool.h`, `src/contur/dispatch/dispatcher_pool.cpp` | Implement real worker pool consuming runtime-owned `threading_config`; preserve deterministic and ownership-transfer invariants. | `test_dispatcher_pool.cpp`, contention/liveness tests |

#### Foundational Test Gate (before parallel runtime expansion)

| Test file | Required cases |
|---|---|
| `src/tests/unit/test_kernel_builder.cpp` | defaults; composition wiring for injected runtime/dispatcher; kernel snapshot remains runtime-agnostic |
| `src/tests/unit/test_mp_dispatcher.cpp` | empty lanes -> `InvalidState` (configured runtime); explicit serial runtime dispatch/tick; custom runtime metadata and call counts |
| `src/tests/unit/test_scheduler.cpp` | null policy path is Result-based (`InvalidState`), no exceptions |
| `src/tests/integration/test_kernel_api.cpp` | runtime/dispatcher `InvalidState` bubbles up through kernel API without exception flow |

#### Execution Rule For Next Iterations

- Implement exactly one step (I1..I7) per iteration.
- Run build + tests after every step (`Build contur2 (Debug)` then `Run Tests (Debug)`).
- Proceed to advanced threaded runtime expansion only after all I-step gates are green.

### Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 11.1 | `threading_config.h` — `HostThreadingConfig` as runtime-owned config (`hostThreadCount`, `deterministicMode`, `workStealing`) | `dispatch/threading_config.h` | — | — | ✅ |
| 11.2 | Runtime strategy abstraction: `i_dispatch_runtime.h` + serial baseline implementation | `dispatch/i_dispatch_runtime.h`, `dispatch/serial_dispatch_runtime.h` | `dispatch/serial_dispatch_runtime.cpp` | `test_mp_dispatcher.cpp` | ✅ |
| 11.3 | `dispatcher_pool.h` — `DispatcherPool` (PIMPL; owns N workers and N dispatcher lanes; consumes runtime config internally) | `dispatch/dispatcher_pool.h` | `dispatch/dispatcher_pool.cpp` | `test_dispatcher_pool.cpp` | ✅ |
| 11.4 | Refactor `mp_dispatcher.h` integration: runtime injection mandatory via DI, no hidden concrete fallback | `dispatch/mp_dispatcher.h` (update) | `dispatch/mp_dispatcher.cpp` (update) | `test_mp_dispatcher.cpp` | ✅ |
| 11.5 | Update `kernel_builder.h/cpp` composition root to wire runtime/dispatcher components (not raw thread-count fields in kernel) | `kernel/kernel_builder.h` (update) | `kernel/kernel_builder.cpp` (update) | `test_kernel_builder.cpp`, `test_kernel_api.cpp` | ✅ |
| 11.6 | Scheduler concurrency model: per-core ready queues + work stealing + ownership handoff invariants | `scheduling/i_scheduler.h` (update), `scheduling/scheduler.h` (update) | `scheduling/scheduler.cpp` (update) | `test_scheduler_concurrent.cpp` | ✅ |
| 11.7 | Policy contract hardening: policies consume snapshots only (no lock ownership, no shared-state mutation) | `scheduling/i_scheduling_policy.h` (update) | policy `*.cpp` audit | `test_policy_contracts.cpp` | ✅ |
| 11.8 | Synchronization split: kernel-internal locks vs simulated `ISyncPrimitive` resources | `sync/i_sync_primitive.h` (update docs), `sync/*.h` audit | `sync/*.cpp` updates | `test_sync_layers.cpp` | ✅ |
| 11.9 | Priority inversion controls for simulated mutex/semaphore (priority inheritance/boost rules) | `sync/mutex.h` (update), `sync/semaphore.h` (update) | `sync/mutex.cpp`, `sync/semaphore.cpp` | `test_priority_inversion.cpp` | ✅ |
| 11.10 | Shared resource arbitration: MMU/page-table critical sections + per-device serialization + IPC channel guards | `memory/*.h` (targeted updates), `io/*.h`, `ipc/*.h` | `memory/*.cpp`, `io/*.cpp`, `ipc/*.cpp` | `test_resource_contention.cpp` | ✅ |
| 11.11 | Deadlock detector v2: simulated wait-for graph (thread-aware) + internal lock-order graph | `sync/deadlock_detector.h` (update) | `sync/deadlock_detector.cpp` (update) | `test_deadlock_detector_concurrent.cpp` | ✅ |
| 11.12 | Deterministic N>1 mode: epoch/barrier checkpoints + stable tie-break ordering for replayability | `dispatch/dispatcher_pool.h` (update), `core/clock.h` (if needed) | `dispatch/dispatcher_pool.cpp` | `test_deterministic_multithread.cpp` | ✅ |
| 11.13 | Thread-aware tracing metadata (worker id, sequence, epoch) for reproducible diagnostics | `tracing/trace_event.h` (update) | `tracing/*.cpp` updates | `test_tracer_concurrent.cpp` | ✅ |

### Acceptance Criteria
- Dispatcher/runtime layer supports configurable `N >= 1` host threads from a single code path.
- Kernel remains composition-focused and runtime-agnostic (no embedded thread-count config fields).
- N=1 behavior remains equivalent to current baseline (no functional regressions).
- N>1 executes processes in parallel with per-core queues and work stealing.
- Scheduling policies remain pure strategy objects (no direct synchronization side effects).
- Shared resources (memory/devices/IPC) are protected against races without global-lock bottlenecks.
- Deadlock detection reports both simulated resource cycles and internal lock-order cycles.
- Deterministic mode reproduces scheduling/trace order for fixed seed/configuration.
- Concurrency tests pass under ThreadSanitizer in debug threading mode.

---

## Phase 12: Tracing (`tracing/`)

**Goal**: Implement the compile-time-toggled tracing subsystem (Observer pattern, zero-cost in Release).

**Dependencies**: Phase 1 (IClock, Event), Phase 10 (Kernel for integration), Phase 11 (concurrent runtime metadata)

### Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 12.1 | `trace_event.h` — `TraceEvent` struct (timestamp, subsystem, operation, details, depth) | `tracing/trace_event.h` | — | — | ✅ |
| 12.2 | `trace_sink.h` — `ITraceSink` interface (write) | `tracing/trace_sink.h` | — | — | ✅ |
| 12.3 | `i_tracer.h` — `ITracer` interface (trace, pushScope, popScope, currentDepth, clock) | `tracing/i_tracer.h` | — | — | ✅ |
| 12.4 | `tracer.h` — `Tracer` (PIMPL; active implementation writing to ITraceSink) | `tracing/tracer.h` | `tracing/tracer.cpp` | `test_tracer.cpp` | ✅ |
| 12.5 | `null_tracer.h` — `NullTracer` (inline no-ops) | `tracing/null_tracer.h` | — | — | ✅ |
| 12.6 | `trace_scope.h` — `TraceScope` RAII guard + `CONTUR_TRACE_SCOPE` / `CONTUR_TRACE` macros | `tracing/trace_scope.h` | — | — | ✅ |
| 12.7 | Sink implementations: `ConsoleSink`, `FileSink`, `BufferSink` | `tracing/console_sink.h` + 2 | `tracing/console_sink.cpp` + 2 | `test_buffer_sink.cpp` | ✅ |
| 12.8 | CMake: `CONTUR2_ENABLE_TRACING` option, `CONTUR_TRACE_ENABLED` define | `src/CMakeLists.txt` (update) | — | — | ✅ |
| 12.9 | Wire tracer into KernelBuilder and inject into all subsystems | update `kernel_builder.cpp` | — | — | ✅ |

### Acceptance Criteria
- Tracer + BufferSink: emit 3 nested scoped events → buffer contains 3 events with depth 0,1,2
- NullTracer: compiles, does nothing, zero overhead
- CONTUR_TRACE_SCOPE macro: compiles to nothing when `CONTUR_TRACE_ENABLED` is not defined
- KernelBuilder with Tracer: run a process, verify trace output appears

---

## Phase 13: Terminal UI (`tui/`)

**Goal**: Define and implement an **external** UI module (MVC) with contracts-first architecture, controller state
machine, history navigation, renderer interfaces, **and a working FTXUI rendering backend** (`FtxuiRenderer` + `FtxuiApp`).

**Dependencies**: Phase 10 (Kernel API), Phase 11 (scheduler lane snapshots), Phase 12 (optional trace metadata)

### Non-Negotiable Boundaries

- UI is **not** part of kernel runtime logic; it is an external consumer module.
- Kernel remains headless and independently testable without TUI linkage.
- UI receives read-only data from a diagnostics module (`IKernelDiagnostics`) and never queries kernel directly.
- UI history is owned by UI controller/store; kernel does not persist UI playback history.
- Rewind/forward operates on UI history only; it does **not** roll back kernel state.
- Any future renderer backend (ANSI/FTXUI/ncurses/etc.) must implement view interfaces only.

### MVC Scope For This Phase

- **Model**: immutable UI snapshot contracts + history records + read-model adapter from kernel state.
- **Controller**: command-driven state machine (`tick`, autoplay start/stop, pause, seek backward/forward by N).
- **View**: interfaces (`IRenderer` + view contracts) plus the concrete FTXUI rendering backend (`FtxuiRenderer`) and app shell (`FtxuiApp`) that wires keyboard input, autoplay timer, and the live kernel log pane.

### Tick/Playback Semantics (Contract Level)

- `tick(n)` advances kernel by `n` dispatch cycles and appends snapshots to history.
- `autoplay(intervalMs, strideN)` repeatedly performs `tick(strideN)` every interval until paused/stopped.
- `pause()` freezes autoplay without mutating kernel state.
- `seekBackward(n)` and `seekForward(n)` move the UI cursor over stored history entries.
- Seeking does not execute kernel logic; only cursor navigation over existing snapshots.

### Data Expansion Requirements

Phase 13 must extend UI-facing snapshot contracts beyond aggregate counts so later views can render without
kernel coupling. Required fields include (at minimum):

- Process list: pid, name, state, priority base/effective, cpu time, lane ownership (if available)
- Scheduler queues: ready/blocked/running snapshots + per-lane ready queues
- Memory summary: virtual slots (total/free), frames (total/free), optional frame ownership map hook
- Tick metadata: current tick, selected policy name, deterministic-mode metadata (read-only)
- Optional trace tail reference for timeline correlation (debug/diagnostics)

### Approved Naming (T1-T3)

The following contract names are frozen for Phase 13 T1-T3 implementation:

| Scope | File | Approved names |
|---|---|---|
| T1 Model DTOs | `src/include/contur/tui/tui_models.h` | `TuiProcessSnapshot`, `TuiSchedulerSnapshot`, `TuiMemorySnapshot`, `TuiSnapshot`, `TuiHistoryEntry` |
| T2 Controller commands | `src/include/contur/tui/tui_commands.h` | `TuiCommandKind`, `TuiCommand`, `TuiPlaybackConfig` |
| Diagnostics module | `src/include/contur/kernel/i_kernel_diagnostics.h`, `src/include/contur/kernel/kernel_diagnostics.h`, `src/contur/kernel/kernel_diagnostics.cpp` | `KernelDiagnosticsSnapshot`, `IKernelDiagnostics`, `KernelDiagnostics`, `captureSnapshot()` |
| T3 Read-model adapter | `src/include/contur/tui/i_kernel_read_model.h`, `src/contur/tui/kernel_read_model.cpp` | `IKernelReadModel`, `KernelReadModel`, `captureSnapshot()` |

Naming constraints:

- All TUI DTO/interface types use `Tui` prefix to avoid collision with kernel domain types.
- `*Snapshot` suffix is used only for immutable read-only state objects.
- `KernelSnapshot` remains kernel-facing and must not be renamed by TUI layer.
- `KernelReadModel` is adapter-only and does not own simulation state.

### Implementation Blueprint (Contracts First)

| Step | Files | What to add/fix | Test gate |
|---|---|---|---|
| T1 | `src/include/contur/tui/tui_models.h` | Define immutable UI DTOs: `TuiProcessSnapshot`, `TuiSchedulerSnapshot`, `TuiMemorySnapshot`, `TuiSnapshot`, `TuiHistoryEntry`. | DTO compile checks + serialization/format unit checks |
| T2 | `src/include/contur/tui/tui_commands.h` | Define controller command contracts: `TuiCommandKind`, `TuiCommand`, `TuiPlaybackConfig` (tick/autoplay/pause/seek with stride `N`). | Command validation tests |
| T3 | `src/include/contur/kernel/i_kernel_diagnostics.h`, `src/include/contur/kernel/kernel_diagnostics.h`, `src/contur/kernel/kernel_diagnostics.cpp`, `src/include/contur/tui/i_kernel_read_model.h`, `src/contur/tui/kernel_read_model.cpp` | Add diagnostics contracts/adapters and make `KernelReadModel` consume `IKernelDiagnostics` (`TUI <- Diagnostics <- KernelSnapshot`). | `test_kernel_diagnostics.cpp`, `test_tui_read_model.cpp` |
| T4 | `src/include/contur/tui/history_buffer.h`, `src/contur/tui/history_buffer.cpp` | Add bounded ring buffer with cursor semantics for backward/forward navigation over snapshot history. | `test_tui_history_buffer.cpp` |
| T5 | `src/include/contur/tui/i_tui_controller.h`, `src/contur/tui/tui_controller.cpp` | Implement MVC controller state machine (Idle/Playing/Paused), tick orchestration, autoplay timing contract, seek APIs. | `test_tui_controller.cpp` |
| T6 | `src/include/contur/tui/i_renderer.h` + view contracts | Keep renderer/view interfaces backend-agnostic (`render(viewModel)`, `clear()`, panel contracts). | Interface compile checks |
| T7 | `src/tests/integration/test_tui_tick_navigation.cpp` | Integration test for `tick(n)`, autoplay pause/resume, and seek behavior against captured history. | Integration gate |
| T8 | `.github/IMPLEMENTATION_PLAN.md`, `.github/instructions/contur2.instructions.md` | Keep architecture notes synchronized (UI external module, no kernel rewind semantics). | Doc review gate |

### CMake Targets

| Target | Sources | Links to | Purpose |
|---|---|---|---|
| `contur2_lib` | `src/contur/**/*.cpp` (excludes `tui/`) | — | Kernel library; zero UI dependency |
| `contur2_tui` | `src/contur/tui/*.cpp` | `contur2_lib` (PUBLIC) | TUI layer; future FTXUI backend goes here |
| `contur2` (app) | `src/app/main.cpp` | `contur2_tui`, `contur2_demos` | Entry point; gets kernel transitively |
| `contur2_unit_tests` | `src/tests/unit/` (excl. `test_tui*`) | `contur2_lib` | Kernel unit tests |
| `contur2_tui_unit_tests` | `src/tests/unit/test_tui*.cpp` | `contur2_tui` | TUI unit tests |
| `contur2_integration_tests` | `src/tests/integration/` (excl. `test_tui*`) | `contur2_lib` | Kernel integration tests |
| `contur2_tui_integration_tests` | `src/tests/integration/test_tui*.cpp` | `contur2_tui` | TUI integration tests |

> **Invariant**: `contur2_lib` must never link or `#include` anything from `contur2_tui`.
> FTXUI (or any other renderer backend) is added as a dependency of `contur2_tui` only.

### Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 13.1 | Define UI model contracts (`TuiProcessSnapshot`, `TuiSchedulerSnapshot`, `TuiMemorySnapshot`, `TuiSnapshot`, `TuiHistoryEntry`) | `tui/tui_models.h` | — | `test_tui_models.cpp` | ✅ |
| 13.2 | Define controller command contracts (`TuiCommandKind`, `TuiCommand`, `TuiPlaybackConfig`) with step size `N` and autoplay interval | `tui/tui_commands.h` | — | `test_tui_commands.cpp` | ✅ |
| 13.3 | Define diagnostics contracts (`KernelDiagnosticsSnapshot`, `IKernelDiagnostics`) | `kernel/i_kernel_diagnostics.h` | — | — | ✅ |
| 13.4 | Implement diagnostics adapter (`KernelDiagnostics`) over `IKernel::snapshot()` | `kernel/kernel_diagnostics.h` | `kernel/kernel_diagnostics.cpp` | `test_kernel_diagnostics.cpp` | ✅ |
| 13.5 | Define/implement read-model adapter from diagnostics to TUI snapshots (`IKernelReadModel`, `KernelReadModel`) | `tui/i_kernel_read_model.h` | `tui/kernel_read_model.cpp` | `test_tui_read_model.cpp` | ✅ |
| 13.6 | Implement bounded UI history buffer with cursor and seek-by-`N` | `tui/history_buffer.h` | `tui/history_buffer.cpp` | `test_tui_history_buffer.cpp` | ✅ |
| 13.7 | Define TUI controller interface (`ITuiController`) | `tui/i_tui_controller.h` | — | — | ✅ |
| 13.8 | Implement controller state machine: `tick(n)`, autoplay, pause, `seekBackward(n)`, `seekForward(n)` | — | `tui/tui_controller.cpp` | `test_tui_controller.cpp` | ✅ |
| 13.9 | Define backend-agnostic renderer interface(s) for MVC view boundary | `tui/i_renderer.h` | — | `test_tui_renderer_contracts.cpp` | ✅ |
| 13.10 | Define view contracts for process/scheduler/memory panels (interfaces only) | `tui/process_view.h`, `tui/scheduler_view.h`, `tui/memory_map_view.h`, `tui/dashboard.h` | — | compile-only contract checks | ✅ |
| 13.11 | Implement FTXUI renderer backend (`FtxuiRenderer` implementing `IRenderer`) | `tui/ftxui_renderer.h` | `tui/ftxui_renderer.cpp` | `test_tui_ftxui_renderer.cpp` | ✅ |
| 13.12 | Implement FTXUI app shell (`FtxuiApp` — keyboard input loop, autoplay timer, log pane integration) | `tui/ftxui_app.h` | `tui/ftxui_app.cpp` | `test_tui_ftxui_integration.cpp` | ✅ |
| 13.13 | Add integration tests for tick playback and history navigation semantics | — | — | `test_tui_tick_navigation.cpp` | ✅ |
| 13.14 | Document library strategy (defer backend choice, keep adapter seam) | `.github/IMPLEMENTATION_PLAN.md` | — | review | ✅ |
| 13.15 | Document strict boundary: UI external module, no kernel rollback from snapshot | `.github/IMPLEMENTATION_PLAN.md`, `.github/instructions/contur2.instructions.md` | — | review | ✅ |

### Acceptance Criteria

- UI architecture is documented and enforced as external to kernel (no kernel-side UI coupling).
- Controller contracts support `tick(n)`, autoplay every `N` ms with stride `n`, pause, seek backward/forward by `n`.
- History navigation is explicit UI-only playback; kernel rollback is not implemented and not implied by API.
- Extended UI snapshot contracts exist for processes, queues, and memory-level data.
- Renderer/view interfaces compile independently from any concrete backend implementation.
- FTXUI backend (`FtxuiRenderer` + `FtxuiApp`) renders the live kernel state and is the entry point launched from `src/app/main.cpp`.
- Unit + integration tests validate controller transitions, playback loop behavior, history cursor semantics, and FTXUI renderer/integration behavior.

---

## Phase 14: App Shell (`app/`) — *FTXUI-based, replaces CLI-menu demos*

**Goal**: Provide a runnable end-user entry point. The original plan for a Debug/Release CLI menu plus a separate
`demos/*.cpp` library was **superseded** during Phase 13 by the FTXUI TUI (see [13.11]–[13.12]): all subsystem demos
are exercised live inside the TUI from sample processes spawned by `src/app/main.cpp`.

**Dependencies**: Phases 10, 11, 12, 13

### Status

- `src/demos/CMakeLists.txt` is retained as an `INTERFACE` library placeholder (no sources). It is kept so that
    targeted standalone demo programs can be added later without restructuring the build, but it is not used today.
- The previously planned `Stepper` / `CONTUR_STEP_MODE` step-mode mechanism is **not implemented**; the FTXUI app
    provides interactive pause/seek/autoplay instead.

### Tasks

| # | Task | Files | Test | Done |
|---|---|---|---|---|
| 14.1 | FTXUI-based app shell launching the simulator with sample processes (replaces planned CLI menu) | `src/app/main.cpp`, `src/app/CMakeLists.txt` | covered by `test_tui_ftxui_integration.cpp` | ✅ |
| 14.2 | Demo kernel builder + sample programs (`makeProgramAddOnePlusOne`, counter loop, CPU-heavy, idle/background NOP loops) inside the app shell | `src/app/main.cpp` | exercised end-to-end at runtime | ✅ |
| 14.3 | Trace dump on shutdown (kernel `BufferSink` flushed to stdout after the TUI exits) | `src/app/main.cpp` | exercised end-to-end at runtime | ✅ |
| 14.4 | `contur2_demos` placeholder CMake target retained for future standalone demos | `src/demos/CMakeLists.txt` | — | ✅ |
| 14.5 | (Optional, future) Re-introduce per-subsystem standalone demo programs (architecture, scheduling, sync, deadlock, IPC, filesystem, multiprocessor, interpreter, userspace) under `src/demos/src/` | `demos/src/*.cpp` | — | |
| 14.6 | (Optional, future) `Stepper` utility + `CONTUR_STEP_MODE` for step-by-step CLI walkthroughs | `demos/include/demos/stepper.h` + `.cpp` | — | |

### Acceptance Criteria
- `contur2` app launches the FTXUI TUI, drives sample processes through the scheduler, and exits cleanly on user quit.
- Kernel trace events are surfaced live in the TUI log pane and dumped to stdout on shutdown.
- The `contur2_demos` target builds (currently as `INTERFACE`) and remains a valid extension point for future demos.

---

## Phase 15: User Space — Native x86 Program Execution (`execution/`)

**Goal**: Make Contur 2 a real **program execution engine** by wiring the existing `IExecutionEngine` strategy with a second concrete engine — `NativeEngine` — that launches a real x86 binary as a host child process and lets the simulator's scheduler drive its lifecycle (suspend / resume / terminate) via host-OS APIs. The whole rest of the kernel (Dispatcher, Scheduler, MMU, FileSystem, IPC, SyscallTable, Tracer) is **already in place** — Phase 15 is a focused, infrastructure-respecting addition: one new engine, one tiny field on `ProcessImage`, one demo, tests.

**End-state acceptance**: a C-compiled `hello` binary (`hello.exe` on Windows, an ELF on Linux x86) is registered with the kernel; `kernel->createProcess(cfg)` returns a PID; the simulator's dispatcher ticks → `NativeEngine` resumes the suspended host process for a budgeted slice of wallclock → suspends it → returns control to the dispatcher → reports exit code on termination. Output of the child is captured via stdout pipe and surfaced through the existing `ITracer` for observability.

> **Why this approach**: the project's full infrastructure (MMU, Scheduler, Dispatcher, SyscallTable, IPC, FileSystem, Kernel facade, KernelBuilder, Tracer, TUI) is already operational. Re-deriving an in-kernel virtual ISA, ELF/CEF loader, libc-CRT, etc. would mean building an entire second OS personality on top of a working one. Following the project's own design (`contur2.instructions.md` §1: *"Dual Execution Engine — InterpreterEngine + NativeEngine"*), the right move is a thin `NativeEngine` that plugs into `IExecutionEngine` exactly the way `InterpreterEngine` already does. No new abstractions; one new strategy plug-in.

**Scope decision**: x86 only. Windows + POSIX (Linux/macOS) hosts are both supported in the same `NativeEngine` class via host-gated `Impl`. The two paths are functionally equivalent at the lifecycle level (spawn → suspend → resume slice → suspend → reap) and behind the same public header.

**Dependencies**: Phase 5 (`IExecutionEngine` seam), Phase 7 (`Dispatcher` already calls `engine.execute`), Phase 10 (`KernelBuilder::withExecutionEngine`), Phase 12 (`ITracer`).

---

### 15.0 What's already there vs what's missing

| Component | Status | Used by Phase 15? |
|---|---|---|
| `IExecutionEngine` interface (`execute`, `halt`, `name`) | ✅ exists | yes — `NativeEngine` implements it |
| `InterpreterEngine` (concrete impl) | ✅ exists | unchanged, kept side-by-side |
| `ExecutionResult` + `StopReason` (BudgetExhausted/ProcessExited/Error/Interrupted/Halted) | ✅ exists | reused 1:1 |
| `Dispatcher::dispatch` calls `engine.execute(*processImage, tickBudget)` | ✅ exists | reused 1:1 |
| `ProcessImage` (PCB + code + RegisterFile) | ✅ exists | adds **one** optional field: `nativePath_` |
| `ProcessConfig` in `IKernel` | ✅ exists | adds **one** optional field: `nativePath` |
| `Kernel::createProcess` flow | ✅ exists | passes `nativePath` through to `ProcessImage` |
| `KernelBuilder::withExecutionEngine` | ✅ exists | demo wires `NativeEngine` instead of `InterpreterEngine` |
| `ITracer` | ✅ exists | engine emits trace events for spawn/suspend/resume/exit |
| `SyscallTable` | ✅ exists | not used by `NativeEngine` (real x86 process makes real Win32 calls; we don't intercept them) |
| `NativeEngine` (concrete impl) | ❌ missing | **this is what Phase 15 adds** |

**That's the full delta.** No new interfaces, no parallel ISA, no new loader, no new privilege model, no syscall ABI work, no MMU changes. One new engine class behind an existing strategy seam, plus one optional field on `ProcessImage`.

### 15.1 Concept

The simulator becomes a **program execution engine** in the literal sense: it owns and controls a real host child process exactly the way an OS owns a user-space process — admit it, allocate it a PID, schedule it, give it CPU time slices, capture its output, observe its termination. The host process is the real x86 program. The simulator is the kernel.

```
+-----------------------------------------------------------------------+
|  Contur 2 simulator (host process)                                    |
|                                                                       |
|   +------- KERNEL -------+         +-------- USER PROCESS --------+   |
|   | KernelBuilder        |         | Real Windows x86 child       |   |
|   | Kernel facade        |         | (e.g. hello.exe)             |   |
|   | Dispatcher           |         |                              |   |
|   | Scheduler            |         | Suspended at spawn time      |   |
|   | SyscallTable         |         | Resumed during dispatch tick |   |
|   | Tracer               |         | Suspended at tick end        |   |
|   +-----+----------------+         +------------+-----------------+   |
|         |                                       ^                     |
|         | Dispatcher::dispatch                  |                     |
|         v                                       |                     |
|   +-----+--------------+   suspend/resume       |                     |
|   | IExecutionEngine   |---- via Win32 -------->+                     |
|   |  (Strategy)        |                                              |
|   |                    |   stdout via pipe ----- captured ----+       |
|   |  - Interpreter (kept, unchanged)                          |       |
|   |  - NativeEngine (NEW — wraps Win32 child process)         v       |
|   +--------------------+                              +---------+     |
|                                                       | Tracer  |     |
|                                                       +---------+     |
+-----------------------------------------------------------------------+
```

The dispatcher does not know it is running a real x86 binary instead of an interpreted Block program. It calls `engine.execute(processImage, tickBudget)` and receives an `ExecutionResult` exactly as before. `NativeEngine` is a strict drop-in for `InterpreterEngine` behind the same interface.

#### Lifecycle mapping (host-equivalent)

| Simulator state / event | Win32 action | POSIX action |
|---|---|---|
| First `execute()` call for a PID | `CreateProcessW` with `CREATE_SUSPENDED` + redirected stdout pipe | `pipe()` + `fork()` → child does `dup2`/`execv`; parent sends `SIGSTOP` |
| Subsequent `execute()` calls | `ResumeThread(mainThread)` → `WaitForSingleObject(process, sliceMs)` → `SuspendThread` | `kill(pid, SIGCONT)` → `nanosleep(sliceMs)` → `kill(pid, SIGSTOP)` |
| `halt(pid)` / process termination | `TerminateProcess` + `CloseHandle` | `kill(pid, SIGKILL)` + `waitpid(pid, …, 0)` to reap zombie |
| Child exits naturally | `WaitForSingleObject(WAIT_OBJECT_0)` + `GetExitCodeProcess` → `R0` | `waitpid(WNOHANG)` returns `pid` → `WIFEXITED`/`WIFSIGNALED` → `R0` |
| stdout bytes available | `PeekNamedPipe` + `ReadFile` from anonymous pipe | non-blocking `read()` from pipe FD with `O_NONBLOCK` |

#### What we deliberately do NOT do

- We do **not** intercept the child's syscalls. The child runs natively and uses Win32 NT syscalls; instrumenting that would require a debugger API (`DebugActiveProcess`) which is out of scope for an educational kernel simulator. Process-level scheduling and stdout capture are sufficient to demonstrate "the kernel runs the program".
- We do **not** touch the simulated MMU/VirtualMemory for native processes. Their memory is owned by the host OS. `Dispatcher::createProcess` still allocates a slot for bookkeeping (so PCB/state machine work uniformly), but it is not used as backing for the child's instructions.
- We do **not** implement the interpreted-side `SystemCall` plumbing in this phase (that remains a separate concern; `SyscallTable` is already wired into the Kernel facade for direct API use, and the interpreter's `Interrupted` state continues to translate into `Scheduler::blockProcess` as today).

### 15.2 Implementation Plan

#### Files added

| Path | Role |
|---|---|
| `src/include/contur/execution/native_engine.h` | Public header for `NativeEngine` (PIMPL, implements `IExecutionEngine`) |
| `src/contur/execution/native_engine.cpp` | Implementation; Win32 + POSIX backends gated by `#if defined(_WIN32)` / `#elif defined(__unix__) || defined(__APPLE__)`; symmetric lifecycle |
| `src/tests/unit/test_native_engine.cpp` | Unit tests for `NativeEngine` (Windows + POSIX cases under host gates) |
| `src/tests/integration/test_native_kernel_flow.cpp` | Integration: `Kernel` + `NativeEngine` runs a host binary to completion (Windows + POSIX cases under gates, plus the C-Lang hello-world demo) |
| `src/tests/fixtures/native_hello.c` | Tiny `puts("hello, contur")` C source compiled by CMake on POSIX hosts to provide a real ELF for the headline integration test |
| `src/tests/fixtures/native_test_paths.h.in` | Generated header carrying the path of the compiled fixture binary into tests |

#### Files extended (minimal, additive)

| Path | Change |
|---|---|
| `src/include/contur/process/process_image.h` | New optional getter `std::string_view nativePath() const`; new constructor overload that accepts a path; back-compat with the existing `vector<Block>` constructor preserved |
| `src/contur/process/process_image.cpp` | Stores `nativePath_` inside `Impl` |
| `src/include/contur/kernel/i_kernel.h` | `ProcessConfig` gains an optional `std::string nativePath` field (default empty) |
| `src/contur/kernel/kernel.cpp` | `Kernel::createProcess` forwards `config.nativePath` to `ProcessImage` |

That is **the entire surface area change**. No new interfaces, no new directory.

#### NativeEngine internal contract

The `Impl` struct holds a host-specific `Child` POD (handles on Windows, `pid_t` + pipe FD on POSIX) plus a process map, a halt set, and the tracer reference. Both backends share the same `execute()` shape:

1. If `pid` ∈ `haltRequested` → terminate, reap, return `ExecutionResult::halted`.
2. If no entry for `pid` → spawn (suspended) and insert.
3. If entry already marked `exited` → write `exitCode` to `R0`, return `ExecutionResult::exited`.
4. Resume the child (Win32 `ResumeThread` / POSIX `kill(SIGCONT)`).
5. Wait the wallclock slice (`tickBudget × tickQuantumMs`) — Win32 `WaitForSingleObject` (interrupted on exit), POSIX `nanosleep` then `waitpid(WNOHANG)`.
6. Suspend the child (best-effort; race window with natural exit is handled by re-polling).
7. Drain stdout; if exited, emit `R0` + return `exited`; otherwise return `budgetExhausted`.

`halt(pid)` records the request and, on POSIX/Win32 alike, terminates and reaps the tracked child immediately so the next `execute()` (or destructor) sees a terminal state.

`name()` returns `"Native"` on every host.

#### POSIX-specific notes

- `fork()` followed by SIGSTOP has a tiny race window where the child can exit before the parent's signal lands. The engine handles this through `waitpid(WNOHANG)` in `pollExitNoHang` — the next `execute()` reaps the child and reports `exited` with the captured code.
- `std::string` allocation is performed in the **parent** before `fork()`; the child only calls async-signal-safe functions (`dup2`, `close`, `execv`, `_exit`) on the path buffer it inherited.
- The pipe read end is configured with `O_NONBLOCK | FD_CLOEXEC` so drains never block and the FD is not leaked into recursive forks.
- The C-Lang hello-world fixture is compiled at CMake configure time when `UNIX AND NOT APPLE` and a C compiler is available (`clang`/`gcc`/`cc`); the resulting absolute path is exposed via the generated `native_test_paths.h`. On Windows or hosts without a C compiler, the fixture path is empty and the test gracefully `GTEST_SKIP`s.

#### Configuration

`NativeEngine` constructor takes:
- `ITracer&` — required, for spawn/resume/suspend/exit/stdout trace events
- `std::uint32_t tickQuantumMs = 5` — wallclock ms per simulation tick (kept small so tests are fast and scheduling is observable)

No CMake flag is needed for the engine itself: Win32 headers come from the standard SDK on Windows; POSIX headers (`<unistd.h>`, `<sys/wait.h>`, `<signal.h>`, `<fcntl.h>`) are part of the system C library on Linux/macOS. The C-fixture compilation is opt-in via toolchain detection in `tests/CMakeLists.txt`.

### 15.3 Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 15.1 | Add `nativePath_` to `ProcessImage::Impl` + getter + ctor overload | `process/process_image.h` (update) | `process/process_image.cpp` (update) | covered by integration tests | ✅ |
| 15.2 | Add `nativePath` to `ProcessConfig`; thread it from `Kernel::createProcess` to `ProcessImage` | `kernel/i_kernel.h` (update) | `kernel/kernel.cpp` (update) | covered by integration tests | ✅ |
| 15.3 | `NativeEngine` PIMPL skeleton implementing `IExecutionEngine` (returns `error` if path empty) | `execution/native_engine.h` | `execution/native_engine.cpp` | `test_native_engine.cpp` | ✅ |
| 15.4 | Win32 backend: `CreateProcessW` + stdout pipe + suspend, tracked in `Child` map | — | `execution/native_engine.cpp` | `test_native_engine.cpp` (Windows) | ✅ |
| 15.5 | Win32 dispatch loop: resume → wait `sliceMs` → suspend or detect exit → drain stdout | — | `execution/native_engine.cpp` | `test_native_engine.cpp` (Windows) | ✅ |
| 15.6 | Win32 `halt(pid)` → `TerminateProcess` + handle cleanup | — | `execution/native_engine.cpp` | `test_native_engine.cpp` (Windows) | ✅ |
| 15.7 | Tracer events for spawn/resume/suspend/exit/stdout | — | `execution/native_engine.cpp` | `BufferSink` assertions in tests | ✅ |
| 15.8 | POSIX backend: `fork`/`execv` + stdout pipe + `SIGSTOP`/`SIGCONT`/`waitpid` (Linux/macOS) | — | `execution/native_engine.cpp` | `test_native_engine.cpp` (POSIX) | ✅ |
| 15.9 | POSIX `halt(pid)` → `SIGKILL` + blocking `waitpid` to reap zombie | — | `execution/native_engine.cpp` | `test_native_engine.cpp` (POSIX) | ✅ |
| 15.10 | C-Lang hello-world fixture: compile `tests/fixtures/native_hello.c` on POSIX hosts via host C compiler | `tests/fixtures/native_test_paths.h.in` | `tests/fixtures/native_hello.c`, `tests/CMakeLists.txt` | — | ✅ |
| 15.11 | Integration: `Kernel` + `NativeEngine` runs to completion, exit code captured (Windows + POSIX cases) | — | — | `test_native_kernel_flow.cpp` | ✅ |
| 15.12 | Integration: kernel runs the compiled C hello-world end-to-end (POSIX) | — | — | `test_native_kernel_flow.cpp` (`KernelRunsCompiledClangHelloWorld`) | ✅ |

### 15.4 Acceptance Criteria

**Windows host (verified on Windows 11)**:
- `cmake --build --preset win-debug` succeeds with no new warnings.
- `ctest --preset win-debug` passes the full Windows suite (the `NativeEngine` adds 18 unit + 11 integration tests on top of the cross-platform baseline).
- A `Kernel` built via `KernelBuilder().withExecutionEngine(std::make_unique<NativeEngine>(*tracer))` admits a process with `config.nativePath = "C:\\Windows\\System32\\hostname.exe"` and `runForTicks` drives it to completion; `KernelSnapshot` reflects 0 processes after exit.
- The simulator's `Tracer` sink records `spawn.ok`/`resume`/`suspend`/`exit` events for the native process in chronological order.
- Captured stdout from the child is non-empty for `hostname.exe`.

**POSIX host (Linux x86 / macOS)**:
- The same `KernelBuilder` configuration runs `/bin/hostname` and `/bin/true` to completion; the latter sets `R0 = 0`, the former captures stdout containing the host name.
- The C-Lang hello-world fixture is compiled at configure time when a host C compiler is available; the integration test `KernelRunsCompiledClangHelloWorld` admits the produced ELF as a Contur process and asserts the captured stdout contains `hello, contur`.
- Tests gated `#elif defined(__unix__) || defined(__APPLE__)` are visible to ctest on POSIX hosts; no Windows-specific code is reachable from the POSIX path and vice versa.

### 15.5 Out-of-scope (not part of Phase 15)

- In-process syscall interception of the native child. Would require attaching as a debugger (`DebugActiveProcess` / `ptrace`); deferred indefinitely.
- Loading or interpreting ELF/PE byte-by-byte — unnecessary; the host OS does that for us.
- Argument passing for `nativePath` — current API stores a single absolute path; `argv` extension can be added later (`std::vector<std::string> nativeArgs` on `ProcessConfig`) without touching `IExecutionEngine`.
- Any new ISA (RV32IM, custom byte-level Contur, etc.). The interpreter's existing `vector<Block>` ISA is unchanged and remains the educational illustration target.

---

## Phase 16: Full Test Suite (`tests/`)

**Goal**: Comprehensive unit and integration test coverage.

**Dependencies**: All previous phases

### Tasks

| # | Task | Files | Done |
|---|---|---|---|
| 16.1 | Audit all existing unit tests, fill coverage gaps via `_extended` suites (buffer_sink, ipc_manager, kernel_diagnostics, message_queue, pipe, scheduler, scheduling_policies, simple_fs, statistics, syscall, tracer, tui_history_buffer, tui_read_model, virtual_memory) | `tests/unit/test_*_extended.cpp` | ✅ |
| 16.2 | Full lifecycle integration test (replaces planned `test_dispatcher_flow.cpp`) | `tests/integration/test_kernel_end_to_end.cpp` | ✅ |
| 16.3 | Program load → execute → verify output (covered by `test_interpreter_engine.cpp` + `test_kernel_end_to_end.cpp`; standalone `test_interpreter_execution.cpp` not added) | covered indirectly | 🔄 |
| 16.4 | `test_kernel_api.cpp` — end-to-end through IKernel | `tests/integration/test_kernel_api.cpp` | ✅ |
| 16.5 | Producer/consumer through pipes + message queues (covered by `test_pipe_extended.cpp` and `test_message_queue_extended.cpp`; dedicated `test_ipc_flow.cpp` not added) | covered indirectly | 🔄 |
| 16.6 | File create/read/write/delete (covered by `test_simple_fs_extended.cpp` + `test_syscall_extended.cpp`; dedicated `test_filesystem_io.cpp` not added) | covered indirectly | 🔄 |
| 16.7 | Coverage report: target 80%+ line coverage | — | |
| 16.8 | `test_scheduler_concurrent.cpp` — per-core queue correctness + work stealing behavior under load | `tests/unit/test_scheduler_concurrent.cpp` | ✅ |
| 16.9 | `test_deadlock_detector_concurrent.cpp` — thread-aware wait-for cycles + internal lock-order cycle detection | `tests/unit/test_deadlock_detector_concurrent.cpp` | ✅ |
| 16.10 | `test_deterministic_multithread.cpp` — identical seed/config produces identical scheduling/trace order in N>1 mode | `tests/integration/test_deterministic_multithread.cpp` | ✅ |
| 16.11 | Stress suite: high-contention memory/device/IPC scenarios with bounded-time liveness checks (partially covered by `test_resource_contention.cpp`; dedicated `test_contention_stress.cpp` not added) | `tests/unit/test_resource_contention.cpp` | 🔄 |
| 16.12 | ThreadSanitizer lane (`tsan-debug`, `tsan-release`, `tsan-gcc-debug`, `tsan-gcc-release`, `tsan-win-*`) for race detection on multithreaded paths | `src/CMakePresets.json` | ✅ |
| 16.13 | Process/scheduling integration suite (added on top of original plan) | `tests/integration/test_process_scheduling_integration.cpp` | ✅ |

### Acceptance Criteria
- `ctest --preset debug` passes all tests with zero failures
- No ASan/UBSan violations
- Coverage ≥ 80% for `contur/` + `include/contur/` code
- `ctest --preset debug-tsan` passes for multithreaded test subsets with zero data-race reports
- N=1 and N>1 deterministic-mode suites are reproducible for fixed seeds/configs

---

## Phase 17: Documentation & CI

**Goal**: Finalize API docs, Doxygen, and CI pipeline.

**Dependencies**: All previous phases

### Tasks

| # | Task | Done |
|---|---|---|
| 17.1 | Add `///` Doxygen comments to all public interfaces | 🔄 (most headers documented; full audit pending) |
| 17.2 | CMake `docs` target (uses `add_custom_target` + Doxygen + `doxygen-awesome-css`, theme overrides via `Doxyfile.in` / `Doxyfile.override.in`) | ✅ |
| 17.3 | GitHub Actions workflow: matrix Clang + GCC, Release + TSAN-Release (see `.github/workflows/ci.yml`) | 🔄 (Release/TSAN-Release axes shipped; Debug axis + ASAN/UBSAN not yet in CI) |
| 17.4 | Coverage step in CI (lcov/gcov or llvm-cov) | [ ] |
| 17.5 | Top-level `README.md` with build instructions and demo screenshots | ✅ |
| 17.6 | Document multithreading runtime architecture (N>=1, per-core queues, work stealing, deterministic mode) in `.github/instructions/contur2.instructions.md` | 🔄 |
| 17.7 | Add CI matrix axis for host-thread counts (at least N=1 and N=4) + TSAN job | 🔄 (TSAN job ✅; explicit N-axis pending) |
| 17.8 | Document lock hierarchy, shared-resource arbitration rules, and deadlock analysis model (simulated + internal) | 🔄 |
| 17.9 | Standalone Doxygen publish workflow (`.github/workflows/docs.yml`) deploying to GitHub Pages | ✅ |

### Acceptance Criteria
- `cmake --build --preset debug --target docs` generates HTML API docs
- CI pipeline passes on push/PR for both compilers
- README accurately describes how to build and run
- CI validates both baseline (N=1) and multithreaded (N>1) modes
- CI includes TSAN validation for the concurrent runtime path

---

## Test Statistics (Phases 0–12)

| Phase | Test File | Test Suites | Tests |
|---|---|---|---|
| 1 | `test_result.cpp` | ResultTest, ResultVoidTest, ErrorCodeTest | 11 |
| 1 | `test_clock.cpp` | SimulationClockTest | 8 |
| 1 | `test_event.cpp` | EventTest | 12 |
| 1 | `test_register_file.cpp` | RegisterFileTest, RegisterNameTest | 12 |
| 2 | `test_physical_memory.cpp` | PhysicalMemoryTest | 12 |
| 2 | `test_page_table.cpp` | PageTableTest | 14 |
| 2 | `test_page_replacement.cpp` | FifoReplacementTest, LruReplacementTest, ClockReplacementTest, OptimalReplacementTest | 20 |
| 2 | `test_mmu.cpp` | MmuTest | 17 |
| 2 | `test_virtual_memory.cpp` | VirtualMemoryTest | 15 |
| 3 | `test_process_state.cpp` | ProcessStateTest | 30 |
| 3 | `test_priority.cpp` | PriorityLevelTest, PriorityTest | 22 |
| 3 | `test_pcb.cpp` | PCBTest | 25 |
| 3 | `test_process_image.cpp` | ProcessImageTest | 22 |
| 4 | `test_alu.cpp` | ALUTest | 32 |
| 4 | `test_cpu.cpp` | CpuTest | 34 |
| 4 | `test_device_manager.cpp` | DeviceManagerTest, NetworkDeviceTest, ConsoleDeviceTest | 28 |
| 5 | `test_interpreter_engine.cpp` | InterpreterEngineTest | 26 |
| 6 | `test_fcfs.cpp` | FcfsPolicyTest | 2 |
| 6 | `test_round_robin.cpp` | RoundRobinPolicyTest | 3 |
| 6 | `test_spn.cpp` | SpnPolicyTest | 2 |
| 6 | `test_srt.cpp` | SrtPolicyTest | 2 |
| 6 | `test_hrrn.cpp` | HrrnPolicyTest | 2 |
| 6 | `test_priority_policy.cpp` | PriorityPolicyTest | 2 |
| 6 | `test_mlfq.cpp` | MlfqPolicyTest | 2 |
| 6 | `test_statistics.cpp` | StatisticsTest | 3 |
| 6 | `test_scheduler.cpp` | SchedulerTest | 5 |
| 7 | `test_mutex.cpp` | MutexTest | 9 |
| 7 | `test_semaphore.cpp` | SemaphoreTest | 7 |
| 7 | `test_deadlock_detector.cpp` | DeadlockDetectorTest | 15 |
| 7 | `test_dispatcher.cpp` | DispatcherTest | 14 |
| 7 | `test_mp_dispatcher.cpp` | DispatchRuntimeTest, MPDispatcherTest | 19 |
| 8 | `test_pipe.cpp` | PipeTest | 6 |
| 8 | `test_shared_memory.cpp` | SharedMemoryTest | 7 |
| 8 | `test_message_queue.cpp` | MessageQueueTest | 7 |
| 8 | `test_ipc_manager.cpp` | IpcManagerTest | 7 |
| 8 | `test_syscall_table.cpp` | SyscallTableTest | 6 |
| 9 | `test_block_allocator.cpp` | BlockAllocatorTest | 6 |
| 9 | `test_simple_fs.cpp` | SimpleFSTest | 8 |
| 10 | `test_kernel_builder.cpp` | KernelBuilderTest | 16 |
| 10 | `test_kernel_api.cpp` | KernelApiIntegrationTest | 4 |
| 11 | `test_threading_config.cpp` | ThreadingConfigTest | 5 |
| 11 | `test_dispatcher_pool.cpp` | DispatcherPoolTest | 7 |
| 11 | `test_scheduler_concurrent.cpp` | SchedulerConcurrentTest | 6 |
| 11 | `test_policy_contracts.cpp` | PolicyContractTest | 3 |
| 11 | `test_sync_layers.cpp` | SyncLayerTest | 3 |
| 11 | `test_priority_inversion.cpp` | PriorityInversionTest | 3 |
| 11 | `test_resource_contention.cpp` | ResourceContentionTest | 5 |
| 11 | `test_deadlock_detector_concurrent.cpp` | DeadlockDetectorConcurrentTest | 4 |
| 11 | `test_deterministic_multithread.cpp` | DeterministicMultithreadIntegrationTest | 1 |
| 11 | `test_tracer_concurrent.cpp` | TracerConcurrentTest | 2 |
| 12 | `test_buffer_sink.cpp` | BufferSinkTest | 2 |
| 12 | `test_tracer.cpp` | TracerTest | 4 |
| | | **62 suites** | **539** |

### Test Statistics (Phases 13–16, post-baseline additions)

| Phase | Test File | Test Suites | Tests |
|---|---|---|---|
| 13 | `test_kernel_diagnostics.cpp` | KernelDiagnosticsTest | 1 |
| 13 | `test_tui_models.cpp` | TuiModelsTest | 2 |
| 13 | `test_tui_commands.cpp` | TuiCommandsTest | 6 |
| 13 | `test_tui_read_model.cpp` | TuiReadModelTest | 4 |
| 13 | `test_tui_history_buffer.cpp` | TuiHistoryBufferTest | 5 |
| 13 | `test_tui_controller.cpp` | TuiControllerTest | 13 |
| 13 | `test_tui_renderer_contracts.cpp` | TuiRendererContractsTest | 3 |
| 13 | `test_tui_ftxui_renderer.cpp` | TuiFtxuiRendererTest | 16 |
| 13 | `test_tui_tick_navigation.cpp` (integration) | TuiTickNavigationIntegrationTest | 2 |
| 13 | `test_tui_ftxui_integration.cpp` (integration) | TuiFtxuiIntegrationTest | 17 |
| 15 | `test_native_engine.cpp` | NativeEngineTest | 18 |
| 15 | `test_native_kernel_flow.cpp` (integration) | NativeKernelFlowTest | 11 |
| 16 | `test_kernel_end_to_end.cpp` (integration) | KernelEndToEndTest | 18 |
| 16 | `test_process_scheduling_integration.cpp` (integration) | ProcessSchedulingIntegrationTest | 10 |
| 16 | `test_buffer_sink_extended.cpp` | BufferSinkExtendedTest | 10 |
| 16 | `test_ipc_manager_extended.cpp` | IpcManagerExtendedTest | 11 |
| 16 | `test_kernel_diagnostics_extended.cpp` | KernelDiagnosticsExtendedTest | 7 |
| 16 | `test_message_queue_extended.cpp` | MessageQueueExtendedTest | 9 |
| 16 | `test_pipe_extended.cpp` | PipeExtendedTest | 9 |
| 16 | `test_scheduler_extended.cpp` | SchedulerExtendedTest | 19 |
| 16 | `test_scheduling_policies_extended.cpp` | SchedulingPoliciesExtendedTest | 47 |
| 16 | `test_simple_fs_extended.cpp` | SimpleFsExtendedTest | 21 |
| 16 | `test_statistics_extended.cpp` | StatisticsExtendedTest | 11 |
| 16 | `test_syscall_extended.cpp` | SyscallExtendedTest | 9 |
| 16 | `test_tracer_extended.cpp` | TracerExtendedTest | 12 |
| 16 | `test_tui_history_buffer_extended.cpp` | TuiHistoryBufferExtendedTest | 14 |
| 16 | `test_tui_read_model_extended.cpp` | TuiReadModelExtendedTest | 9 |
| 16 | `test_virtual_memory_extended.cpp` | VirtualMemoryExtendedTest | 9 |
| | | **Phase 13:** 69 · **Phase 15:** 29 · **Phase 16:** 225 | **Grand total: 862** |

---

## Summary Timeline

```
Phase 0:  Scaffolding              ████                 ✅  (12 tasks)
Phase 1:  Foundation (core+arch)   ████████             ✅  (9 tasks,  43 tests)
Phase 2:  Memory                   ████████████         ✅  (8 tasks,  78 tests)
Phase 3:  Process                  ████████             ✅  (5 tasks,  99 tests)
Phase 4:  CPU + I/O                ████████████         ✅  (7 tasks,  94 tests)
Phase 5:  Interpreter              ████████             ✅  (3 tasks,  26 tests)
Phase 6:  Scheduling               ████████████████     ✅  (11 tasks, 23 tests)
Phase 7:  Dispatch + Sync          ████████████████     ✅  (8 tasks,  64 tests)
Phase 8:  IPC + Syscalls           ████████████         ✅  (8 tasks,  33 tests)
Phase 9:  File System              ████████████         ✅  (6 tasks,  14 tests)
Phase 10: Kernel                   ████████             ✅  (4 tasks,  20 tests)
Phase 11: Host MT Runtime          ████████████         ✅  (13 tasks, 39 tests)
Phase 12: Tracing                  ████████             ✅  (9 tasks,  6 tests)
Phase 13: TUI (incl. FTXUI)        ████████████████     ✅  (15 tasks, 69 tests)
Phase 14: TUI app shell            ████████             🔄  (replaced original CLI-menu demos; FTXUI app in `src/app/main.cpp`)
Phase 15: User Space (native)      ████████████         ✅  (12 tasks, 29 tests)
Phase 16: Tests                    ████████████████     🔄  (extended/integration suites in place; coverage report + dedicated flow tests pending)
Phase 17: Docs + CI                ████████             🔄  (Doxygen target + docs.yml ✅ · README ✅ · GCC×Clang Release CI ✅ · Debug-axis + coverage + N-axis pending)

Total: 862 tests passing across Phases 0–16
```
