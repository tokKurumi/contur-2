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
machine, history navigation, and renderer interfaces. View rendering implementation is intentionally deferred.

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
- **View**: interfaces only (`IRenderer` + view contracts). No concrete rendering implementation in this phase.

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

### Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 13.1 | Define UI model contracts (`TuiProcessSnapshot`, `TuiSchedulerSnapshot`, `TuiMemorySnapshot`, `TuiSnapshot`, `TuiHistoryEntry`) | `tui/tui_models.h` | — | `test_tui_models.cpp` | |
| 13.2 | Define controller command contracts (`TuiCommandKind`, `TuiCommand`, `TuiPlaybackConfig`) with step size `N` and autoplay interval | `tui/tui_commands.h` | — | `test_tui_commands.cpp` | |
| 13.3 | Define diagnostics contracts (`KernelDiagnosticsSnapshot`, `IKernelDiagnostics`) | `kernel/i_kernel_diagnostics.h` | — | — | ✅ |
| 13.4 | Implement diagnostics adapter (`KernelDiagnostics`) over `IKernel::snapshot()` | `kernel/kernel_diagnostics.h` | `kernel/kernel_diagnostics.cpp` | `test_kernel_diagnostics.cpp` | ✅ |
| 13.5 | Define/implement read-model adapter from diagnostics to TUI snapshots (`IKernelReadModel`, `KernelReadModel`) | `tui/i_kernel_read_model.h` | `tui/kernel_read_model.cpp` | `test_tui_read_model.cpp` | ✅ |
| 13.5 | Implement bounded UI history buffer with cursor and seek-by-`N` | `tui/history_buffer.h` | `tui/history_buffer.cpp` | `test_tui_history_buffer.cpp` | |
| 13.6 | Define TUI controller interface (`ITuiController`) | `tui/i_tui_controller.h` | — | — | |
| 13.7 | Implement controller state machine: `tick(n)`, autoplay, pause, `seekBackward(n)`, `seekForward(n)` | — | `tui/tui_controller.cpp` | `test_tui_controller.cpp` | |
| 13.8 | Define backend-agnostic renderer interface(s) for MVC view boundary | `tui/i_renderer.h` | — | `test_tui_renderer_contracts.cpp` | |
| 13.9 | Define view contracts for process/scheduler/memory panels (interfaces only) | `tui/process_view.h`, `tui/scheduler_view.h`, `tui/memory_map_view.h`, `tui/dashboard.h` | — | compile-only contract checks | |
| 13.10 | Add integration tests for tick playback and history navigation semantics | — | — | `test_tui_tick_navigation.cpp` | |
| 13.11 | Document library strategy (defer backend choice, keep adapter seam) | `.github/IMPLEMENTATION_PLAN.md` | — | review | ✅ |
| 13.12 | Document strict boundary: UI external module, no kernel rollback from snapshot | `.github/IMPLEMENTATION_PLAN.md`, `.github/instructions/contur2.instructions.md` | — | review | ✅ |

### Acceptance Criteria

- UI architecture is documented and enforced as external to kernel (no kernel-side UI coupling).
- Controller contracts support `tick(n)`, autoplay every `N` ms with stride `n`, pause, seek backward/forward by `n`.
- History navigation is explicit UI-only playback; kernel rollback is not implemented and not implied by API.
- Extended UI snapshot contracts exist for processes, queues, and memory-level data.
- Renderer/view interfaces compile independently from any concrete backend implementation.
- Unit + integration tests validate controller transitions, playback loop behavior, and history cursor semantics.

---

## Phase 14: Demos + CLI (`demos/` + `app/`)

**Goal**: Interactive console menu with demo programs for each subsystem. Step-by-step in Debug, continuous in Release.

**Dependencies**: Phases 10, 11, 12, 13

### Tasks

| # | Task | Files | Test | Done |
|---|---|---|---|---|
| 14.1 | `stepper.h` / `stepper.cpp` — `Stepper` utility (CONTUR_STEP_MODE toggle) | `demos/include/demos/stepper.h`, `demos/src/stepper.cpp` | — | |
| 14.2 | `demos.h` — all demo function declarations | `demos/include/demos/demos.h` | — | |
| 14.3 | `demo_context.cpp` — `DemoContext` (PIMPL, bundles Kernel + subsystem references) | `demos/src/demo_context.cpp` | — | |
| 14.4 | `demo_architecture.cpp` — registers, instruction set, fetch-decode-execute | `demos/src/demo_architecture.cpp` | — | |
| 14.5 | `demo_process.cpp` — process creation, priority, state transitions | `demos/src/demo_process.cpp` | — | |
| 14.6 | `demo_memory.cpp` — MMU, virtual memory, page replacement | `demos/src/demo_memory.cpp` | — | |
| 14.7 | `demo_scheduling.cpp` — all 7 scheduling algorithms comparison | `demos/src/demo_scheduling.cpp` | — | |
| 14.8 | `demo_synchronization.cpp` — mutex, semaphore, critical section | `demos/src/demo_synchronization.cpp` | — | |
| 14.9 | `demo_deadlock.cpp` — deadlock detection and prevention | `demos/src/demo_deadlock.cpp` | — | |
| 14.10 | `demo_ipc.cpp` — pipes, shared memory, message queues | `demos/src/demo_ipc.cpp` | — | |
| 14.11 | `demo_filesystem.cpp` — file system operations | `demos/src/demo_filesystem.cpp` | — | |
| 14.12 | `demo_multiprocessor.cpp` — multiprocessor scheduling | `demos/src/demo_multiprocessor.cpp` | — | |
| 14.13 | `demo_interpreter.cpp` — bytecode interpreter step-through | `demos/src/demo_interpreter.cpp` | — | |
| 14.14 | `demo_native.cpp` — native process management (stub until Phase 15) | `demos/src/demo_native.cpp` | — | |
| 14.15 | `main.cpp` — CLI menu loop, KernelBuilder wiring, demo dispatch | `app/main.cpp` | — | |

### Acceptance Criteria
- All demos run in Release mode without pausing
- All demos pause at each step in Debug mode (`CONTUR_STEP_MODE`)
- User can navigate menu, run any demo, return to menu
- `q` during step-by-step skips remaining steps in current demo

---

## Phase 15: Native Execution Engine (`execution/`)

**Goal**: Manage real OS child processes under the simulator's scheduler via platform APIs.

**Dependencies**: Phase 5 (IExecutionEngine interface), Phase 10 (Kernel), Phase 11 (host runtime topology)

### Tasks

| # | Task | Header | Source | Test | Done |
|---|---|---|---|---|---|
| 15.1 | `native_engine.h` — `NativeEngine` (PIMPL; platform-abstract real-process management) | `execution/native_engine.h` | `execution/native_engine.cpp` | `test_native_engine.cpp` | |
| 15.2 | POSIX impl in `Impl`: `fork/exec`, `SIGSTOP/SIGCONT`, `waitpid` | Inside `native_engine.cpp` (`#if defined(__unix__)`) | — | — | |
| 15.3 | Windows impl in `Impl`: `CreateProcess`, `SuspendThread/ResumeThread`, `TerminateProcess` | Inside `native_engine.cpp` (`#elif defined(_WIN32)`) | — | — | |
| 15.4 | Integration: `NativeEngine` in `KernelBuilder`, run external binary under scheduler control | — | — | `test_native_integration.cpp` | |
| 15.5 | Update `demo_native.cpp` with real functionality | `demos/src/demo_native.cpp` | — | — | |

### Acceptance Criteria
- NativeEngine on Linux: launches `/bin/echo hello`, captures exit code
- NativeEngine: halt terminates child process
- Scheduler controls native process via suspend/resume within tick budget

---

## Phase 16: Full Test Suite (`tests/`)

**Goal**: Comprehensive unit and integration test coverage.

**Dependencies**: All previous phases

### Tasks

| # | Task | Files | Done |
|---|---|---|---|
| 16.1 | Audit all existing unit tests, fill coverage gaps | `tests/unit/test_*.cpp` | |
| 16.2 | `test_dispatcher_flow.cpp` — full lifecycle integration test | `tests/integration/test_dispatcher_flow.cpp` | |
| 16.3 | `test_interpreter_execution.cpp` — program load → execute → verify output | `tests/integration/test_interpreter_execution.cpp` | |
| 16.4 | `test_kernel_api.cpp` — end-to-end through IKernel | `tests/integration/test_kernel_api.cpp` | |
| 16.5 | `test_ipc_flow.cpp` — producer/consumer through pipes + message queues | `tests/integration/test_ipc_flow.cpp` | |
| 16.6 | `test_filesystem_io.cpp` — file create/read/write/delete through syscalls | `tests/integration/test_filesystem_io.cpp` | |
| 16.7 | Coverage report: target 80%+ line coverage | — | |
| 16.8 | `test_scheduler_concurrent.cpp` — per-core queue correctness + work stealing behavior under load | `tests/unit/test_scheduler_concurrent.cpp` | |
| 16.9 | `test_deadlock_detector_concurrent.cpp` — thread-aware wait-for cycles + internal lock-order cycle detection | `tests/unit/test_deadlock_detector_concurrent.cpp` | |
| 16.10 | `test_deterministic_multithread.cpp` — identical seed/config produces identical scheduling/trace order in N>1 mode | `tests/integration/test_deterministic_multithread.cpp` | |
| 16.11 | Stress suite: high-contention memory/device/IPC scenarios with bounded-time liveness checks | `tests/integration/test_contention_stress.cpp` | |
| 16.12 | ThreadSanitizer lane (`debug-tsan`) for race detection on multithreaded paths | build/test preset update | |

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
| 17.1 | Add `///` Doxygen comments to all public interfaces | [ ] |
| 17.2 | CMake `doxygen_add_docs` target | [ ] |
| 17.3 | GitHub Actions workflow: matrix (Clang + GCC) × (Debug + Release), test, sanitizers | [ ] |
| 17.4 | Coverage step in CI (lcov/gcov or llvm-cov) | [ ] |
| 17.5 | README.md for src/ with build instructions and demo screenshots | [ ] |
| 17.6 | Document multithreading runtime architecture (N>=1, per-core queues, work stealing, deterministic mode) | [ ] |
| 17.7 | Add CI matrix axis for host-thread counts (at least N=1 and N=4) + TSAN job | [ ] |
| 17.8 | Document lock hierarchy, shared-resource arbitration rules, and deadlock analysis model (simulated + internal) | [ ] |

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
| 2 | `test_mmu.cpp` | MmuTest | 14 |
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
| 10 | `test_kernel_builder.cpp` | KernelBuilderTest | 14 |
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
| 12 | `test_tracer.cpp` | TracerTest | 3 |
| | | **62 suites** | **533** |

---

## Summary Timeline

```
Phase 0:  Scaffolding              ████                 ✅  (12 tasks)
Phase 1:  Foundation (core+arch)   ████████             ✅  (9 tasks,  43 tests)
Phase 2:  Memory                   ████████████         ✅  (8 tasks,  75 tests)
Phase 3:  Process                  ████████             ✅  (5 tasks,  99 tests)
Phase 4:  CPU + I/O                ████████████         ✅  (7 tasks,  94 tests)
Phase 5:  Interpreter              ████████             ✅  (3 tasks,  26 tests)
Phase 6:  Scheduling               ████████████████     ✅  (11 tasks, 23 tests)
Phase 7:  Dispatch + Sync          ████████████████     ✅  (8 tasks,  64 tests)
Phase 8:  IPC + Syscalls           ████████████         ✅  (8 tasks,  33 tests)
Phase 9:  File System              ████████████         ✅  (6 tasks,  14 tests)
Phase 10: Kernel                   ████████             ✅  (4 tasks,  18 tests)
Phase 11: Host MT Runtime          ████████████         ✅  (13 tasks, 39 tests)
Phase 12: Tracing                  ████████             ✅  (9 tasks,  6 tests)
Phase 13: TUI                      ████████████
Phase 14: Demos + CLI              ████████████████
Phase 15: Native Engine            ████████
Phase 16: Tests                    ████████████
Phase 17: Docs + CI                ████████

Total: 533 tests passing (Phases 0–12)
```
