# Contur 2 — OS Kernel Simulator & Interpreter — Copilot Instructions

## Project Overview

Contur 2 is a ground-up rewrite of the Contur educational OS simulator. It models a **real OS kernel** capable of two execution modes:

1. **Interpreted mode** — an internal bytecode interpreter emulates x86-like programs step-by-step (educational, fully portable)
2. **Native mode** — the kernel manages real child processes on the host OS via platform abstractions (`fork/exec` on POSIX, `CreateProcess` on Windows)

This dual-mode architecture lets students compile programs with external compilers (GCC, Clang, etc.) and run them under the simulator's scheduler, while also studying instruction-level execution through the built-in interpreter.

## Purpose

Teaching aid for understanding:
- Computer architecture (registers, instruction fetch-decode-execute cycle)
- Process lifecycle (creation, scheduling, context switching, termination)
- Process priorities and priority-based scheduling
- Memory management (MMU, virtual memory, paging, swap in/out)
- Scheduling algorithms: FCFS, Round Robin, SPN, SRT, HRRN, Dynamic Priority, Multilevel Feedback Queue
- Multiprocessor scheduling with load balancing
- Synchronization primitives (mutexes, semaphores, critical sections)
- I/O device abstraction and interrupt handling
- IPC (inter-process communication): pipes, shared memory, message queues
- System calls interface (syscall table, user-kernel boundary)
- Deadlock detection and prevention (wait-for graph, banker's algorithm)
- Page replacement algorithms (FIFO, LRU, Clock, Optimal)
- Basic file system simulation (inodes, directories, block allocation)
- System-level tracing and diagnostics (hierarchical call tracing, event logging)
- Real-time visualization of OS state (process queues, memory maps, scheduler timeline)

---

## Architecture

### Directory Structure

```
contur/
├── .github/
│   └── instructions/
│       └── contur2.instructions.md         # THIS FILE — src/ instructions
├── .gitignore
├── .clang-format
├── .clang-tidy
├── .vscode/
│   ├── c_cpp_properties.json
│   ├── launch.json
│   └── tasks.json
└── src/
    ├── CMakeLists.txt                       # Root CMake (project-level)
    ├── CMakePresets.json                    # CMake presets (Debug/Release/Test)
    ├── build.sh                            # Build driver script
    ├── app/
    │   ├── CMakeLists.txt
    │   └── main.cpp                        # Entry point — CLI menu
    ├── CMakeLists.txt                      # Library target: contur2_lib
    ├── include/
    │   └── contur/
    │       ├── core/                       # Core abstractions & interfaces
    │       │   ├── types.h                 # Common types, aliases, forward declarations
    │       │   ├── error.h                 # Error codes, Result<T> type
    │       │   ├── clock.h                 # IClock interface + SimulationClock
    │       │   └── event.h                 # Event / Observer infrastructure
    │       ├── arch/                       # Architecture / hardware layer
    │       │   ├── instruction.h           # Instruction enum class
    │       │   ├── interrupt.h             # Interrupt enum class
    │       │   ├── register_file.h         # RegisterFile (16 registers)
    │       │   ├── block.h                 # Block — single memory cell
    │       │   └── isa.h                   # ISA constants & helpers
    │       ├── memory/                     # Memory subsystem
    │       │   ├── i_memory.h              # IMemory interface
    │       │   ├── physical_memory.h       # PhysicalMemory (RAM simulation)
    │       │   ├── i_mmu.h                 # IMMU interface
    │       │   ├── mmu.h                   # MMU implementation
    │       │   ├── i_virtual_memory.h      # IVirtualMemory interface
    │       │   ├── virtual_memory.h        # VirtualMemory implementation
    │       │   └── page_table.h            # PageTable for paging simulation
    │       ├── process/                    # Process subsystem
    │       │   ├── i_process.h             # IProcess interface
    │       │   ├── process.h               # Process implementation
    │       │   ├── pcb.h                   # PCB (Process Control Block)
    │       │   ├── process_image.h         # ProcessImage (full in-memory repr)
    │       │   ├── priority.h              # Priority levels, policies
    │       │   └── state.h                 # ProcessState enum class
    │       ├── execution/                  # Execution engines
    │       │   ├── i_execution_engine.h    # IExecutionEngine interface
    │       │   ├── interpreter_engine.h    # Bytecode interpreter
    │       │   ├── native_engine.h         # Real OS process manager
    │       │   └── execution_context.h     # Shared execution context
    │       ├── cpu/                        # CPU simulation
    │       │   ├── i_cpu.h                 # ICPU interface
    │       │   ├── cpu.h                   # CPU implementation
    │       │   └── alu.h                   # ALU — arithmetic/logic unit
    │       ├── scheduling/                 # Scheduling subsystem
    │       │   ├── i_scheduler.h           # IScheduler interface
    │       │   ├── scheduler.h             # Scheduler (strategy host)
    │       │   ├── i_scheduling_policy.h   # ISchedulingPolicy interface
    │       │   ├── fcfs_policy.h           # First Come First Served
    │       │   ├── round_robin_policy.h    # Round Robin
    │       │   ├── spn_policy.h            # Shortest Process Next
    │       │   ├── srt_policy.h            # Shortest Remaining Time
    │       │   ├── hrrn_policy.h           # Highest Response Ratio Next
    │       │   ├── priority_policy.h       # Dynamic Priority
    │       │   ├── mlfq_policy.h           # Multilevel Feedback Queue
    │       │   └── statistics.h            # Execution statistics & prediction
    │       ├── dispatch/                   # Dispatcher subsystem
    │       │   ├── i_dispatcher.h          # IDispatcher interface
    │       │   ├── dispatcher.h            # Uniprocessor dispatcher
    │       │   └── mp_dispatcher.h         # Multiprocessor dispatcher
    │       ├── sync/                       # Synchronization primitives
    │       │   ├── i_sync_primitive.h      # ISyncPrimitive interface
    │       │   ├── mutex.h                 # Mutex
    │       │   ├── semaphore.h             # Counting semaphore
    │       │   ├── critical_section.h      # Critical section
    │       │   └── deadlock_detector.h     # Wait-for graph + banker's algorithm
    │       ├── ipc/                        # Inter-process communication
    │       │   ├── i_ipc_channel.h         # IIpcChannel interface
    │       │   ├── pipe.h                  # Unidirectional byte stream
    │       │   ├── shared_memory.h         # Named shared memory region
    │       │   ├── message_queue.h         # Typed message passing
    │       │   └── ipc_manager.h           # IPC registry & lifecycle
    │       ├── syscall/                    # System calls interface
    │       │   ├── syscall_table.h         # Syscall dispatch table
    │       │   ├── syscall_handler.h       # ISyscallHandler interface
    │       │   └── syscall_ids.h           # Syscall number enum class
    │       ├── fs/                         # File system simulation
    │       │   ├── i_filesystem.h          # IFileSystem interface
    │       │   ├── simple_fs.h             # SimpleFS — inode-based implementation
    │       │   ├── inode.h                 # Inode structure
    │       │   ├── directory_entry.h       # Directory entry
    │       │   ├── file_descriptor.h       # File descriptor table
    │       │   └── block_allocator.h       # Disk block allocation (bitmap)
    │       ├── io/                         # I/O subsystem
    │       │   ├── i_device.h              # IDevice interface
    │       │   ├── console_device.h        # Console output device
    │       │   ├── network_device.h        # Network (LAN) simulation
    │       │   └── device_manager.h        # Device registry & dispatch
    │       ├── tui/                        # Terminal UI / Visualization
    │       │   ├── i_renderer.h            # IRenderer interface
    │       │   ├── dashboard.h             # Main dashboard layout
    │       │   ├── process_view.h          # Process state table / state diagram
    │       │   ├── memory_map_view.h       # Physical/virtual memory map
    │       │   ├── scheduler_view.h        # Ready/blocked queue visualization
    │       │   └── ansi.h                  # ANSI escape code helpers
    │       ├── tracing/                    # Tracing & diagnostics subsystem
    │       │   ├── i_tracer.h              # ITracer interface
    │       │   ├── tracer.h                # Tracer implementation (active in Debug+CONTUR_TRACE)
    │       │   ├── null_tracer.h           # NullTracer — no-op stub (Release / trace disabled)
    │       │   ├── trace_event.h           # TraceEvent — structured trace record
    │       │   ├── trace_scope.h           # TraceScope — RAII indentation guard
    │       │   └── trace_sink.h            # ITraceSink — output target (console, file, buffer)
    │       └── kernel/                     # Kernel — top-level API
    │           ├── i_kernel.h              # IKernel interface
    │           ├── kernel.h                # Kernel implementation
    │           └── kernel_builder.h        # Builder / DI container
    ├── contur/                             # Implementation sources (.cpp)
    │   ├── core/
    │   ├── arch/
    │   ├── memory/
    │   ├── process/
    │   ├── execution/
    │   ├── cpu/
    │   ├── scheduling/
    │   ├── dispatch/
    │   ├── sync/
    │   ├── ipc/
    │   ├── syscall/
    │   ├── fs/
    │   ├── io/
    │   ├── tui/
    │   ├── tracing/
    │   └── kernel/
    ├── demos/
    │   ├── CMakeLists.txt
    │   ├── include/
    │   │   └── demos/
    │   │       └── demos.h
    │   └── src/
    │       ├── demo_context.cpp
    │       ├── demo_architecture.cpp
    │       ├── demo_process.cpp
    │       ├── demo_memory.cpp
    │       ├── demo_scheduling.cpp
    │       ├── demo_synchronization.cpp
    │       ├── demo_ipc.cpp
    │       ├── demo_filesystem.cpp
    │       ├── demo_deadlock.cpp
    │       ├── demo_multiprocessor.cpp
    │       ├── demo_interpreter.cpp
    │       └── demo_native.cpp
    └── tests/
        ├── CMakeLists.txt
        ├── unit/
        │   ├── test_register_file.cpp
        │   ├── test_physical_memory.cpp
        │   ├── test_process.cpp
        │   ├── test_scheduler.cpp
        │   ├── test_mmu.cpp
        │   ├── test_cpu.cpp
        │   ├── test_priority.cpp
        │   ├── test_page_replacement.cpp
        │   ├── test_deadlock_detector.cpp
        │   ├── test_ipc.cpp
        │   ├── test_syscall.cpp
        │   └── test_filesystem.cpp
        └── integration/
            ├── test_dispatcher_flow.cpp
            ├── test_interpreter_execution.cpp
            ├── test_kernel_api.cpp
            └── test_ipc_flow.cpp
```

### Code Organization

| Directory | Purpose |
|---|---|
| `src/include/contur/` | **Public headers** — interfaces, class declarations, PIMPL forward decls |
| `src/contur/` | **Implementations** — `.cpp` files, PIMPL `Impl` structs (mirrors `include/contur/` structure) |
| `src/include/contur/tracing/` | **Tracing subsystem** — `ITracer`, `TraceScope`, sinks; compiled conditionally |
| `src/include/contur/ipc/` | **IPC** — pipes, shared memory, message queues |
| `src/include/contur/syscall/` | **System calls** — syscall table, handler dispatch |
| `src/include/contur/fs/` | **File system** — inode-based FS simulation |
| `src/include/contur/tui/` | **Terminal UI** — ANSI-based dashboard, process/memory/scheduler views |
| `src/app/` | **Entry point** — `main.cpp` with interactive CLI menu |
| `src/demos/` | **Demo functions** — one per subsystem, receives `DemoContext&`; step-by-step in Debug |
| `src/tests/` | **Tests** — unit and integration tests (Google Test or Catch2) |

### Layer Dependency Graph (Dependency Inversion Applied)

```
                    ┌────────────────────────────────────────┐
                    │              app/main.cpp              │
                    │         (creates KernelBuilder,        │
                    │          wires dependencies)           │
                    └──────────────┬─────────────────────────┘
                                   │ depends on
                    ┌──────────────▼─────────────────────────┐
                    │         kernel/ (IKernel)              │
                    │    KernelBuilder assembles all parts   │
                    └──────────────┬─────────────────────────┘
                                   │ depends on interfaces
          ┌────────────────────────┼────────────────────────┐
          │                        │                        │
  ┌───────▼─────────┐   ┌──────────▼───────────┐   ┌────────▼─────────┐
  │ dispatch/       │   │ scheduling/          │   │ sync/            │
  │ (IDispatcher)   │   │ (IScheduler,         │   │ (ISyncPrimitive) │
  │                 │   │  ISchedulingPolicy)  │   │                  │
  └───────┬─────────┘   └──────────┬───────────┘   └──────────────────┘
          │                        │
  ┌───────▼─────────┐   ┌──────────▼───────────┐
  │ execution/      │   │ process/             │
  │ (IExecEngine)   │   │ (IProcess, PCB,      │
  │                 │   │  ProcessImage,       │
  │                 │   │  Priority)           │
  └───────┬─────────┘   └──────────┬───────────┘
          │                        │
  ┌───────▼──────────┐   ┌─────────▼───────────┐
  │ cpu/             │   │ memory/             │
  │ (ICPU, ALU)      │   │ (IMemory, IMMU,     │
  │                  │   │  IVirtualMemory)    │
  └───────┬──────────┘   └──────────┬──────────┘
          │                         │
  ┌───────▼─────────────────────────▼────────────┐
  │              arch/ + core/                   │
  │ (Block, Instruction, Interrupt, RegisterFile,│
  │  IClock, types, error, event)                │
  └──────────────────────────────────────────────┘
          │
  ┌───────▼───────────┐
  │     io/           │
  │ (IDevice,         │
  │  DeviceManager)   │
  └───────────────────┘
```

**Rule**: Upper layers depend on **interfaces** (`I*` headers) from lower layers, never on concrete implementations. Concrete classes are wired together in `KernelBuilder`.

---

## Key Design Decisions

### 1. Dual Execution Engine

```cpp
// Pure interface — no coupling to implementation
class IExecutionEngine {
public:
    virtual ~IExecutionEngine() = default;
    virtual ExecutionResult execute(ProcessImage& process, std::size_t tickBudget) = 0;
    virtual void halt(ProcessId pid) = 0;
    virtual std::string_view name() const noexcept = 0;
};
```

- **InterpreterEngine** — fetches `Block` instructions from simulated `IMemory`, decodes via `ICPU`, executes step-by-step. Fully deterministic and portable. Students can inspect each cycle.
- **NativeEngine** — wraps host OS process management (`fork/exec` on POSIX, `CreateProcess` on Windows). Allows running real ELF/PE binaries under the simulator's scheduler. The kernel controls time slices by sending `SIGSTOP`/`SIGCONT` (POSIX) or `SuspendThread`/`ResumeThread` (Windows).

Both engines implement the same `IExecutionEngine` interface and are interchangeable at runtime via the Strategy pattern.

### 2. Process Priorities

Process priority is a first-class concept with a dedicated subsystem:

```cpp
enum class PriorityLevel : std::int8_t {
    Realtime  = 0,   // Highest
    High      = 1,
    AboveNormal = 2,
    Normal    = 3,   // Default
    BelowNormal = 4,
    Low       = 5,
    Idle      = 6    // Lowest
};

struct Priority {
    PriorityLevel base;      // Assigned at creation
    PriorityLevel effective;  // May be boosted/decayed dynamically
    std::int32_t  nice;       // Fine-grained adjustment (-20..+19, Unix-style)
};
```

- **Static priority** — set at process creation, never changes
- **Dynamic priority** — adjusted by the scheduler based on wait time, I/O behavior, starvation prevention
- **Priority inversion protection** — priority inheritance protocol in sync primitives
- **Priority-to-timeslice mapping** — higher priority → larger or more frequent time slices (configurable per policy)

### 3. Smart Pointers Everywhere

| Ownership type | Smart pointer | Example |
|---|---|---|
| **Sole ownership** | `std::unique_ptr<T>` | Kernel owns Dispatcher; Dispatcher owns VirtualMemory; CPU owns ALU |
| **Shared ownership** | `std::shared_ptr<T>` | ProcessImage's code segment (shared between VirtualMemory and MMU during swap) |
| **Non-owning reference** | `T&` or `std::reference_wrapper<T>` | Scheduler references ICPU; CPU references IMemory |
| **Optional non-owning** | Prefer `std::optional<std::reference_wrapper<T>>`; use `T*` only when unavoidable | PCB's optional register file link |
| **Observer/weak reference** | `std::weak_ptr<T>` | Event subscribers; process handles returned to user code |

**Rules**:
- **No owning raw pointers**. Every `new` must be wrapped in `std::make_unique` or `std::make_shared`.
- **No `delete`** anywhere in the codebase.
- Prefer `std::unique_ptr` by default; upgrade to `std::shared_ptr` only when shared ownership is genuinely needed.
- Avoid raw pointers where practical, even for non-owning relationships: prefer `T&`, `std::reference_wrapper<T>`,
    `std::optional<std::reference_wrapper<T>>`, iterators, IDs/handles, or `std::span` for ranges.
- Use non-owning raw pointers only when they are clearly the simplest/most efficient representation for a nullable
    relationship, or when required by API/ABI interop.
- Factory functions return `std::unique_ptr<Interface>`.
- Collections own their elements: `std::vector<std::unique_ptr<ProcessImage>>` for VirtualMemory slots.

### 4. Interfaces and Dependency Inversion (DIP)

Every subsystem boundary is defined by a pure abstract interface (`I*` classes):

```
IKernel          — public API for process/sync operations
IDispatcher      — orchestrates process lifecycle
IScheduler       — manages state queues and scheduling decisions
ISchedulingPolicy — pluggable scheduling algorithm (Strategy)
IExecutionEngine — runs programs (interpreter or native)
ICPU             — fetch-decode-execute cycle
IMMU             — address translation, swap in/out
IMemory          — linear addressable memory
IVirtualMemory   — virtual address space management
IDevice          — I/O device abstraction
ISyncPrimitive   — mutex, semaphore, critical section
IClock           — simulation time source
```

**Interface rules**:
- Only pure virtual methods + virtual destructor
- No data members, no constructors (besides `= default`)
- Declared in separate `i_*.h` headers
- Implementations include the interface header, never the reverse
- All cross-layer dependencies use interface references/pointers

### 5. PIMPL (Pointer to Implementation)

Every non-trivial class uses PIMPL to provide:
- **Compilation firewall** — changing implementation details doesn't recompile dependents
- **ABI stability** — safe for incremental builds during development
- **Clean public headers** — headers expose only the interface, no private fields

```cpp
// kernel.h (public header)
#pragma once
#include "contur/kernel/i_kernel.h"
#include <memory>

namespace contur {

class Kernel final : public IKernel {
public:
    explicit Kernel(/* injected dependencies */);
    ~Kernel() override;

    // Non-copyable, movable
    Kernel(const Kernel&) = delete;
    Kernel& operator=(const Kernel&) = delete;
    Kernel(Kernel&&) noexcept;
    Kernel& operator=(Kernel&&) noexcept;

    // IKernel interface
    ProcessHandle createProcess(const ProcessConfig& config) override;
    void terminateProcess(ProcessId pid) override;
    // ...

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace contur
```

```cpp
// kernel.cpp (implementation)
#include "contur/kernel/kernel.h"
#include "contur/dispatch/i_dispatcher.h"
// ... other includes

namespace contur {

struct Kernel::Impl {
    std::unique_ptr<IDispatcher> dispatcher;
    std::unique_ptr<ISyncManager> syncManager;
    // all private state lives here
};

Kernel::Kernel(/* deps */)
    : impl_(std::make_unique<Impl>())
{
    // wire dependencies
}

Kernel::~Kernel() = default;
Kernel::Kernel(Kernel&&) noexcept = default;
Kernel& Kernel::operator=(Kernel&&) noexcept = default;

// ... method implementations delegating to impl_->

} // namespace contur
```

### 6. Builder / DI Container

`KernelBuilder` assembles the entire system, wiring all dependencies:

```cpp
auto kernel = KernelBuilder()
    .withMemory(std::make_unique<PhysicalMemory>(memorySize))
    .withCpu(std::make_unique<Cpu>(/* ... */))
    .withMmu(std::make_unique<Mmu>(/* ... */))
    .withSchedulingPolicy(std::make_unique<RoundRobinPolicy>(timeSlice))
    .withExecutionEngine(std::make_unique<InterpreterEngine>(/* ... */))
    .withDevices({
        std::make_unique<ConsoleDevice>(),
        std::make_unique<NetworkDevice>()
    })
    .build();  // returns std::unique_ptr<IKernel>
```

This is the **only place** where concrete types are mentioned together. The rest of the codebase works exclusively through interfaces.

> **Configuration**: Simulation parameters (memory size, time slice, number of processors, scheduling algorithm name) can be loaded from a simple JSON or key-value config file and fed into `KernelBuilder`. This keeps demo programs free from hardcoded values and allows students to experiment without recompiling.

### 7. Tracing & Diagnostics

A dedicated tracing subsystem provides hierarchical, indented call-trace output for debugging and educational purposes. It is a **pure observer** — it only listens to kernel events, never modifies kernel state.

#### Architecture

```
┌──────────────────────────────────────────────────────────┐
│                      Kernel                              │
│  ┌──────────┐  ┌───────────┐  ┌──────────┐  ┌─────────┐  │
│  │Dispatcher│  │ Scheduler │  │   MMU    │  │   CPU   │  │
│  └────┬─────┘  └─────┬─────┘  └────┬─────┘  └────┬────┘  │
│       │ emit         │ emit        │ emit        │ emit  │
│       ▼              ▼             ▼             ▼       │
│  ┌──────────────────────────────────────────────────┐    │
│  │           Event<TraceEvent> bus                  │    │
│  └──────────────────────┬───────────────────────────┘    │
└─────────────────────────┼────────────────────────────────┘
                          │ subscribe (read-only)
              ┌───────────▼───────────┐
              │     ITracer           │
              │  (external observer)  │
              └───────────┬───────────┘
                          │ write
              ┌───────────▼───────────┐
              │     ITraceSink        │
              │  (console / file /    │
              │   in-memory buffer)   │
              └───────────────────────┘
```

#### Compile-Time Control

Tracing is controlled by the CMake option `CONTUR2_ENABLE_TRACING`:

- **`ON`** (default in Debug) — `Tracer` is active, events are captured, output goes to configured `ITraceSink`
- **`OFF`** (default in Release) — `NullTracer` is injected, all trace calls are no-ops, **zero runtime overhead** (calls are optimized out by the compiler)

```cmake
option(CONTUR2_ENABLE_TRACING "Enable kernel tracing (Debug only)" OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CONTUR2_ENABLE_TRACING ON CACHE BOOL "" FORCE)
endif()
if(CONTUR2_ENABLE_TRACING)
    target_compile_definitions(contur2_lib PUBLIC CONTUR_TRACE_ENABLED)
endif()
```

#### Key Types

```cpp
// Structured trace record emitted by kernel subsystems
struct TraceEvent {
    Tick              timestamp;     // From IClock
    std::string_view  subsystem;     // "Dispatcher", "Scheduler", "CPU", "MMU", etc.
    std::string_view  operation;     // "dispatch", "contextSwitch", "swapIn", "fetch", etc.
    std::string       details;       // Human-readable context (process name, address, etc.)
    std::uint32_t     depth;         // Nesting level for indentation
};

// RAII guard that increments depth on construction, decrements on destruction
// Produces hierarchical "step into / step out" tracing automatically
class TraceScope {
public:
    TraceScope(ITracer& tracer, std::string_view subsystem, std::string_view operation);
    ~TraceScope();
    // Non-copyable, non-movable
};

// Convenience macro — compiles to nothing when tracing is disabled
#ifdef CONTUR_TRACE_ENABLED
    #define CONTUR_TRACE_SCOPE(tracer, subsystem, op) \
        contur::TraceScope _trace_scope_##__LINE__((tracer), (subsystem), (op))
    #define CONTUR_TRACE(tracer, subsystem, op, details) \
        (tracer).trace({(tracer).clock().now(), (subsystem), (op), (details), (tracer).currentDepth()})
#else
    #define CONTUR_TRACE_SCOPE(tracer, subsystem, op) ((void)0)
    #define CONTUR_TRACE(tracer, subsystem, op, details) ((void)0)
#endif
```

#### ITracer Interface

```cpp
class ITracer {
public:
    virtual ~ITracer() = default;
    virtual void trace(const TraceEvent& event) = 0;
    virtual void pushScope(std::string_view subsystem, std::string_view operation) = 0;
    virtual void popScope() = 0;
    virtual std::uint32_t currentDepth() const noexcept = 0;
    virtual const IClock& clock() const noexcept = 0;
};
```

- **`Tracer`** — real implementation: formats events with indentation, writes to `ITraceSink`
- **`NullTracer`** — no-op: all methods are empty inline, optimized away

#### Output Example (indented hierarchical trace)

```
[T=0001] Kernel::createProcess
[T=0001]   Dispatcher::allocateVirtualMemory
[T=0001]     VirtualMemory::allocateSlot → slot=3
[T=0001]   Dispatcher::dispatch
[T=0001]     Scheduler::enqueue → pid=1 state=New→Ready
[T=0002]     Scheduler::selectNext → pid=1 (RoundRobin)
[T=0002]   Dispatcher::executeProcess
[T=0002]     MMU::swapIn → pid=1 vaddr=3 raddr=0x40
[T=0002]     CPU::fetch → addr=0x40 instr=Mov
[T=0002]     CPU::decode → Mov r1, 42
[T=0002]     CPU::execute → r1=42
[T=0003]     CPU::fetch → addr=0x41 instr=Add
[T=0003]     CPU::decode → Add r1, r2
[T=0003]     CPU::execute → r1=52
```

#### Integration with Kernel

The `ITracer` is injected into the Kernel via `KernelBuilder`. Kernel passes it down to Dispatcher, Scheduler, CPU, MMU as a non-owning reference (`ITracer&`). Each subsystem emits `TraceEvent` records through the tracer, but **never depends on whether tracing is active** — the `NullTracer` silently discards everything.

```cpp
auto kernel = KernelBuilder()
    .withTracer(std::make_unique<Tracer>(
        std::make_unique<ConsoleSink>(),   // or FileSink("trace.log")
        clock
    ))
    // ... other components ...
    .build();
```

#### Sink Implementations

| Sink | Purpose |
|---|---|
| `ConsoleSink` | Prints to `std::cout` with ANSI color codes (subsystem = color) |
| `FileSink` | Writes to a log file (plain text, no colors) |
| `BufferSink` | Stores in `std::vector<TraceEvent>` for programmatic inspection (tests) |

### 8. Step-by-Step Execution Mode

Demo programs support two execution modes controlled by the build configuration:

| Build | Mode | Behavior |
|---|---|---|
| **Release** | Continuous | Demos run from start to finish without pausing |
| **Debug** | Step-by-step | After each logical step, execution pauses and waits for user input (Enter to continue, `q` to skip to end) |

#### Implementation

Step-by-step mode is implemented via a lightweight `Stepper` utility — **not** baked into the kernel. It lives in the `demos/` layer and wraps demo logic:

```cpp
// demos/include/demos/stepper.h
namespace contur::demos {

class Stepper {
public:
    explicit Stepper(std::string_view demoName);

    // Displays step description and pauses in Debug mode.
    // In Release mode, only prints the description (no pause).
    void step(std::string_view description);

    // Check whether user requested skip-to-end
    bool isSkipping() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace contur::demos
```

#### Compile-Time Control

```cmake
target_compile_definitions(contur2_demos
    PRIVATE
        $<$<CONFIG:Debug>:CONTUR_STEP_MODE>
)
```

Inside `Stepper::step()`:

```cpp
void Stepper::step(std::string_view description)
{
    std::cout << "\n── Step: " << description << " ──\n";
#ifdef CONTUR_STEP_MODE
    if (!impl_->skipping) {
        std::cout << "[Press Enter to continue, 'q' + Enter to skip to end]\n";
        std::string input;
        std::getline(std::cin, input);
        if (!input.empty() && input[0] == 'q') {
            impl_->skipping = true;
        }
    }
#endif
}
```

#### Usage in Demos

```cpp
void demoScheduling(DemoContext& ctx)
{
    Stepper step("Scheduling Algorithms");

    step.step("Creating 5 processes with different priorities");
    // ... create processes ...

    step.step("Running FCFS scheduling");
    // ... run FCFS ...

    step.step("Switching to Round Robin (time slice = 3)");
    // ... switch policy, run ...

    step.step("Comparing SPN vs SRT results");
    // ... run both, display comparison ...
}
```

#### Interaction with Tracing

When **both** tracing and step-by-step mode are active (Debug build with `CONTUR_TRACE_ENABLED` + `CONTUR_STEP_MODE`), the student gets a full educational experience:
1. `Stepper` pauses before each high-level step
2. The student presses Enter
3. The step executes, and the `Tracer` prints the detailed hierarchical call trace in real time
4. `Stepper` pauses again for the next step

This creates a "debugger-like" walkthrough of the entire OS simulation.

---

## Design Patterns

| Pattern | Where used | Purpose |
|---|---|---|
| **PIMPL** | All non-trivial classes | Compilation firewall, ABI stability, clean headers |
| **Strategy** | `ISchedulingPolicy` implementations | Swap scheduling algorithms at runtime |
| **Abstract Factory** | `KernelBuilder` | Assembles system from interface-bound components |
| **Observer** | `Event<Args...>` template | Decouple event producers (scheduler, dispatcher) from consumers (statistics, logging) |
| **Command** | `Instruction` + `Block` | Each instruction is a command object executed by CPU |
| **State** | `ProcessState` enum class + transitions in Dispatcher | Formalized process state machine with validated transitions |
| **Template Method** | `IDispatcher::dispatch()` base flow | Define skeleton; subclasses override `scheduleProcess()` / `executeProcess()` |
| **Facade** | `IKernel` | Single entry point hiding Dispatcher, Scheduler, MMU, etc. |
| **Composite** | `DeviceManager` holds `vector<unique_ptr<IDevice>>` | Uniform device management |
| **Flyweight** | `Block` (small value objects in memory arrays) | Memory-efficient instruction storage |
| **Proxy** | `ProcessHandle` (returned to user code, wraps `weak_ptr`) | Safe external reference to internal process |
| **Null Object** | `NullTracer` vs `Tracer` | Zero-cost tracing in Release; full tracing in Debug |
| **Chain of Responsibility** | `TraceScope` RAII nesting | Automatic depth tracking for hierarchical trace output |
| **Mediator** | `IpcManager` | Centralized IPC channel registry; processes never know about each other directly |
| **Iterator** | `DirectoryIterator` in `SimpleFS` | Traversing directory entries uniformly |
| **Registry** | `SyscallTable` dispatch map | Maps syscall IDs to handler functions without `switch` chains |

---

## Component Specifications

### core/types.h — Common Types

```cpp
namespace contur {

using ProcessId     = std::uint32_t;
using MemoryAddress = std::uint32_t;
using Tick          = std::uint64_t;
using RegisterValue = std::int32_t;
using DeviceId      = std::uint16_t;

constexpr ProcessId     INVALID_PID  = 0;
constexpr MemoryAddress NULL_ADDRESS = 0xFFFFFFFF;

} // namespace contur
```

### core/error.h — Error Handling

```cpp
namespace contur {

enum class ErrorCode : std::int32_t {
    Ok = 0,
    OutOfMemory,
    InvalidPid,
    InvalidAddress,
    DivisionByZero,
    InvalidInstruction,
    SegmentationFault,
    DeviceError,
    ProcessNotFound,
    PermissionDenied,
    Timeout,
    DeadlockDetected,
};

template <typename T>
class Result {
public:
    // Holds either a T value or an ErrorCode
    // Inspired by Rust's Result<T, E>
    static Result ok(T value);
    static Result error(ErrorCode code);
    bool isOk() const noexcept;
    bool isError() const noexcept;
    const T& value() const;
    ErrorCode errorCode() const;
};

// Specialization for void
template <>
class Result<void>;

} // namespace contur
```

### core/clock.h — Simulation Clock

```cpp
namespace contur {

class IClock {
public:
    virtual ~IClock() = default;
    virtual Tick now() const noexcept = 0;
    virtual void tick() = 0;
    virtual void reset() = 0;
};

// No more global static Timer — clock is injected as a dependency
class SimulationClock final : public IClock {
public:
    SimulationClock();
    ~SimulationClock() override;
    Tick now() const noexcept override;
    void tick() override;
    void reset() override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace contur
```

### arch/ — Architecture Layer

Scoped enums with explicit underlying types:

```cpp
enum class Instruction : std::uint8_t {
    Nop = 0,
    Mov, Add, Sub, Mul, Div,
    And, Or, Xor,
    ShiftLeft, ShiftRight,
    Compare,
    JumpEqual, JumpNotEqual, JumpGreater, JumpLess,
    JumpGreaterEqual, JumpLessEqual,
    Push, Pop,
    Call, Return,
    ReadMemory, WriteMemory,
    Interrupt,
    Halt
};

enum class Interrupt : std::int32_t {
    Ok          = 0,
    Error       = -1,
    Exit        = 16,
    SystemCall  = 11,
    DivByZero   = 3,
    DeviceIO    = 254,
    NetworkIO   = 32,
    Timer       = 64,
    PageFault   = 14,
};

enum class Register : std::uint8_t {
    R0 = 0, R1, R2, R3, R4, R5, R6, R7,
    R8, R9, R10, R11, R12, R13,
    ProgramCounter = 14,
    StackPointer   = 15,
};
```

`RegisterFile` replaces `SysReg` — uses `std::array<RegisterValue, 16>` internally, no inner class needed.

### process/state.h — Process State Machine

```cpp
enum class ProcessState : std::uint8_t {
    New,
    Ready,
    Running,
    Blocked,
    Suspended,      // Swapped out
    Terminated,
};

// Validated transitions — enforced at runtime
bool isValidTransition(ProcessState from, ProcessState to);
```

### process/pcb.h — Process Control Block

Uses composition instead of deep inheritance:

```cpp
class PCB {
public:
    // Read-only accessors
    ProcessId id() const noexcept;
    const std::string& name() const noexcept;
    ProcessState state() const noexcept;
    const Priority& priority() const noexcept;
    Tick arrivalTime() const noexcept;
    Tick totalCpuTime() const noexcept;

    // Mutable operations (only accessible to Dispatcher/Scheduler via interface)
    void setState(ProcessState newState);
    void setPriority(Priority priority);
    void addCpuTime(Tick ticks);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

Design rationale:
- `PCB` is a standalone class containing all process metadata via composition
- `ProcessImage` contains a `PCB` + code segment + register file (composition, not inheritance)
- Timing data lives inside `PCB` as a `ProcessTiming` aggregate

### scheduling/i_scheduling_policy.h — Pluggable Algorithms

```cpp
class ISchedulingPolicy {
public:
    virtual ~ISchedulingPolicy() = default;
    virtual std::string_view name() const noexcept = 0;
    virtual ProcessId selectNext(
        const std::vector<const PCB*>& readyQueue,
        const IClock& clock
    ) const = 0;
    virtual bool shouldPreempt(
        const PCB& running,
        const PCB& candidate,
        const IClock& clock
    ) const = 0;
};
```

Each algorithm (FCFS, RR, SPN, SRT, HRRN, Priority, MLFQ) is a separate class implementing this interface. Algorithms are unit-testable in isolation.

### execution/native_engine.h — Real Process Management

```cpp
class NativeEngine final : public IExecutionEngine {
public:
    explicit NativeEngine(/* platform abstraction */);
    ~NativeEngine() override;

    ExecutionResult execute(ProcessImage& process, std::size_t tickBudget) override;
    void halt(ProcessId pid) override;
    std::string_view name() const noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

Platform-specific code is isolated in `Impl`:
- **POSIX** (`#ifdef __unix__`): `fork()`, `exec()`, `waitpid()`, `kill(SIGSTOP/SIGCONT)`
- **Windows** (`#ifdef _WIN32`): `CreateProcess()`, `SuspendThread()`, `ResumeThread()`, `TerminateProcess()`

The engine translates host OS signals/events into the simulator's `ProcessState` transitions.

### 9. IPC (Inter-Process Communication)

Three IPC mechanisms, all behind a common `IIpcChannel` interface:

```cpp
class IIpcChannel {
public:
    virtual ~IIpcChannel() = default;
    virtual Result<std::size_t> write(std::span<const std::byte> data) = 0;
    virtual Result<std::size_t> read(std::span<std::byte> buffer) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const noexcept = 0;
    virtual std::string_view name() const noexcept = 0;
};
```

| Mechanism | Class | Semantics |
|---|---|---|
| **Pipe** | `Pipe` | Unidirectional byte stream; bounded circular buffer; blocks when full/empty |
| **Shared Memory** | `SharedMemory` | Named region mapped into multiple processes' address spaces; lock-free reads |
| **Message Queue** | `MessageQueue` | Typed FIFO; `send(Message)` / `receive()` with optional priority ordering |

`IpcManager` is the registry — creates, looks up, and destroys channels by name. Processes access IPC only through syscalls (never direct references to channels).

### 10. System Calls API

A formalized user-kernel boundary:

```cpp
enum class SyscallId : std::uint16_t {
    Exit           = 0,
    Write          = 1,
    Read           = 2,
    Fork           = 3,
    Exec           = 4,
    Wait           = 5,
    Open           = 10,
    Close          = 11,
    CreatePipe     = 20,
    SendMessage    = 21,
    ReceiveMessage = 22,
    ShmCreate      = 23,
    ShmAttach      = 24,
    MutexLock      = 30,
    MutexUnlock    = 31,
    SemWait        = 32,
    SemSignal      = 33,
    GetPid         = 40,
    GetTime        = 41,
    Yield          = 42,
};

class ISyscallHandler {
public:
    virtual ~ISyscallHandler() = default;
    virtual Result<RegisterValue> handle(
        SyscallId id,
        std::span<const RegisterValue> args,
        ProcessImage& caller
    ) = 0;
};
```

`SyscallTable` maps `SyscallId` → handler function (std::function or interface pointer). The CPU's `Interrupt::SystemCall` triggers dispatch through the table. This cleanly separates user-mode instruction execution from kernel-mode services.

### 11. Deadlock Detection & Prevention

Integrated into `sync/`:

```cpp
class DeadlockDetector {
public:
    // Register resource acquisition/release
    void onAcquire(ProcessId pid, ResourceId resource);
    void onRelease(ProcessId pid, ResourceId resource);
    void onWait(ProcessId pid, ResourceId resource);

    // Detection: cycle in wait-for graph
    [[nodiscard]] bool hasDeadlock() const;
    [[nodiscard]] std::vector<ProcessId> getDeadlockedProcesses() const;

    // Prevention: banker's algorithm
    [[nodiscard]] bool isSafeState(
        const std::vector<ResourceAllocation>& current,
        const std::vector<ResourceAllocation>& maximum
    ) const;
};
```

The detector maintains a **wait-for graph** (adjacency list). On each `onWait()`, it runs cycle detection (DFS). Optionally, the kernel can run **banker's algorithm** before granting resource requests to prevent deadlocks proactively.

### 12. Page Replacement Algorithms

Extends `memory/` with pluggable page replacement:

```cpp
class IPageReplacementPolicy {
public:
    virtual ~IPageReplacementPolicy() = default;
    virtual std::string_view name() const noexcept = 0;
    virtual FrameId selectVictim(const PageTable& pageTable) = 0;
    virtual void onAccess(FrameId frame) = 0;   // Notify policy of page access
    virtual void onLoad(FrameId frame) = 0;     // Notify policy of new page load
};
```

| Algorithm | Class | How it selects victim |
|---|---|---|
| **FIFO** | `FifoReplacement` | Evicts oldest loaded page (queue order) |
| **LRU** | `LruReplacement` | Evicts least recently accessed page (timestamp or stack) |
| **Clock** | `ClockReplacement` | Circular scan with reference bit; second-chance |
| **Optimal** | `OptimalReplacement` | Evicts page not used for longest future time (requires known access sequence; educational only) |

### 13. File System Simulation

A minimal inode-based file system for educational purposes:

```cpp
class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    virtual Result<FileDescriptor> open(const std::string& path, OpenMode mode) = 0;
    virtual Result<std::size_t> read(FileDescriptor fd, std::span<std::byte> buffer) = 0;
    virtual Result<std::size_t> write(FileDescriptor fd, std::span<const std::byte> data) = 0;
    virtual Result<void> close(FileDescriptor fd) = 0;
    virtual Result<void> mkdir(const std::string& path) = 0;
    virtual Result<void> remove(const std::string& path) = 0;
    virtual Result<std::vector<DirectoryEntry>> listDir(const std::string& path) = 0;
    virtual Result<InodeInfo> stat(const std::string& path) = 0;
};
```

Internals of `SimpleFS`:
- **Inode table** — fixed-size array of inodes (type, size, block pointers, timestamps)
- **Block allocator** — bitmap-based allocation of fixed-size disk blocks
- **Directory entries** — name → inode mapping within directory inodes
- **File descriptor table** — per-process; maps small integers to open file state (inode + offset)

The FS operates on a simulated disk (a `std::vector<std::array<std::byte, BLOCK_SIZE>>`).

### 14. Terminal UI / Visualization

An ANSI-based terminal dashboard for real-time visualization of OS state:

```cpp
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void render(const KernelSnapshot& snapshot) = 0;
    virtual void clear() = 0;
};

struct KernelSnapshot {
    std::vector<ProcessInfo>     processes;     // pid, name, state, priority
    MemoryMapSnapshot            memoryMap;     // frame allocation, page tables
    SchedulerSnapshot            scheduler;     // ready/blocked/running queues
    Tick                         currentTick;
};
```

| View | What it shows |
|---|---|
| `ProcessView` | Table of processes with state, priority, CPU time; optional state-transition diagram |
| `MemoryMapView` | Physical frames as a grid; color-coded by owning process; swap status indicators |
| `SchedulerView` | Ready queue contents; Gantt-chart-style timeline of past scheduling decisions |
| `Dashboard` | Composite layout combining all views; refreshes after each simulation tick |

All rendering uses ANSI escape codes via `ansi.h` helpers (cursor movement, colors, box drawing). No external TUI library dependency — pure standard C++ with terminal escape sequences.

---

## Code Quality Guidelines

### Compiler & Standard
- **Compiler**: Clang (primary), GCC (secondary/CI). Must compile cleanly with both.
- **Standard**: C++20 (`-std=c++20`)
- **Warnings**: `-Wall -Wextra -Wpedantic -Werror` in CI; `-Wall -Wextra -Wpedantic` in development
- **Sanitizers**: Enable ASan + UBSan in Debug builds (`-fsanitize=address,undefined`)

### Cross-Platform Portability
- Clang is the primary compiler (available on Linux, macOS, Windows via LLVM)
- All platform-specific code is isolated behind interfaces (e.g., `NativeEngine::Impl`)
- Use `#if defined(__unix__)` / `#elif defined(_WIN32)` for platform branching, only inside `.cpp` files
- Never use compiler-specific extensions in headers
- Use `<cstdint>` types everywhere, no platform-specific integer types
- Filesystem paths use `/` (CMake handles conversion)

### Header Rules
- `#pragma once` for header guards
- Headers contain **only**: class declarations, interface definitions, `enum class`, `constexpr`, `using` aliases, inline trivial getters
- **No implementation in headers** (except trivial one-liners and templates)
- **No `#include` of implementation details** — use forward declarations + PIMPL
- Include order: `<system>` → `<third-party>` → `"contur/core/*"` → `"contur/subsystem/*"`

### No Global Mutable State
- **No `static` mutable variables**
- Clock is an injectable dependency (`IClock`)
- All state is owned by objects with clear lifetimes

### Error Handling
- Use `Result<T>` for operations that can fail (no exceptions for control flow)
- Use `assert()` for programming errors / invariant violations (debug only)
- Never ignore error returns silently

### Memory Safety
- **No owning raw pointers** — `std::unique_ptr` / `std::shared_ptr` everywhere
- **No `new` / `delete`** — use `std::make_unique` / `std::make_shared`
- Prefer references/wrappers/containers over raw pointers where possible; for optional non-owning links, prefer
    `std::optional<std::reference_wrapper<T>>`.
- Keep non-owning raw pointers only when needed for interoperability or a clear low-level/nullability use case.
- **No C-style arrays for owned data** — use `std::array` or `std::vector`
- **No C-style casts** — use `static_cast`, `dynamic_cast`, `reinterpret_cast` (sparingly)
- **No `void*`** — use templates or `std::any` / `std::variant` when type erasure is needed
- `const` correctness enforced on all methods and parameters

---

## Code Style

### Namespace
All code lives under `namespace contur { }`. No `using namespace std;` or `using namespace contur;` in headers. Implementation files may use `using namespace contur;` at the top.

### Naming Conventions

| Element | Convention | Example |
|---|---|---|
| **Namespaces** | `snake_case` | `contur`, `contur::scheduling` (if sub-namespaces are used) |
| **Classes / Structs** | `PascalCase` | `PhysicalMemory`, `ProcessImage`, `RoundRobinPolicy` |
| **Interfaces** | `I` + `PascalCase` | `IScheduler`, `IMemory`, `IClock` |
| **Enum classes** | `PascalCase` type, `PascalCase` values | `ProcessState::Running`, `Instruction::Add` |
| **Methods** | `camelCase` | `createProcess()`, `selectNext()`, `swapIn()` |
| **Member variables** | `camelCase` + trailing `_` | `impl_`, `timeSlice_`, `readyQueue_` |
| **Local variables** | `camelCase` | `realAddr`, `nextProcess`, `tickCount` |
| **Constants** | `constexpr` + `UPPER_SNAKE_CASE` | `MAX_PROCESSES`, `DEFAULT_TIME_SLICE` |
| **Template params** | `PascalCase` | `typename T`, `typename Policy` |
| **File names** | `snake_case.h` / `snake_case.cpp` | `physical_memory.h`, `round_robin_policy.cpp` |

### Formatting
- **Indentation**: 4 spaces (no tabs)
- **Line length**: 120 characters max
- **Braces**: Allman style for class/function definitions (opening brace on new line); K&R for control flow (`if`, `for`, `while` — opening brace on same line)
- **Include paths**: always use `"contur/..."` prefix
- **`.clang-format`** enforces style automatically — always run formatting before committing

```yaml
# .clang-format (key settings)
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 120
BreakBeforeBraces: Custom
BraceWrapping:
  AfterClass: true
  AfterFunction: true
  AfterNamespace: false
  BeforeElse: true
  AfterControlStatement: false
NamespaceIndentation: None
SortIncludes: CaseInsensitive
IncludeBlocks: Regroup
```

### Modern C++ Practices
- `enum class` instead of plain `enum` — no namespace pollution
- `std::string_view` for non-owning string references
- Structured bindings (`auto [key, value] = ...`) where they improve readability
- `[[nodiscard]]` on functions whose return value must be checked (especially `Result<T>`)
- `[[maybe_unused]]` for intentionally unused parameters
- `constexpr` functions for compile-time computations (ISA helpers, state transition validation)
- Range-based `for` loops with `const auto&`
- `std::optional` for optional values; for optional non-owning object links, prefer
    `std::optional<std::reference_wrapper<T>>`
- `noexcept` on destructors, move operations, and simple getters
- Trailing return types (`auto foo() -> ReturnType`) only when needed for `decltype`
- `= default` / `= delete` for special member functions
- Prefer `std::format` (C++20) for string formatting; fall back to `<fmt/format.h>` if stdlib support is lacking

### Class Design

```
public:      // Types, constructors, interface methods
protected:   // Virtual methods for subclass override points
private:     // Impl pointer, helper methods
```

- One class per header/source pair (exceptions: small related types in one header)
- **No `friend` classes** — use interfaces and accessor methods instead
- **Composition over inheritance** — inherit only for interface implementation
- **Rule of Zero** — rely on PIMPL + smart pointers; compiler-generates special members
- **Rule of Five** when PIMPL is used — explicitly default move ops in `.cpp` after `Impl` is defined
- `final` on leaf classes that are not designed for inheritance

---

## Build System

### CMake Configuration

```cmake
cmake_minimum_required(VERSION 3.20)
project(contur2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prefer Clang
if(NOT CMAKE_CXX_COMPILER)
    find_program(CLANG_CXX clang++)
    if(CLANG_CXX)
        set(CMAKE_CXX_COMPILER "${CLANG_CXX}")
    endif()
endif()

add_subdirectory(contur)    # contur2_lib static library
add_subdirectory(demos)
add_subdirectory(app)

# Optional: tests
option(CONTUR2_BUILD_TESTS "Build unit and integration tests" ON)
if(CONTUR2_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

### CMake Presets

```json
{
    "version": 6,
    "configurePresets": [
        {
            "name": "debug",
            "displayName": "Debug (Clang)",
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build/debug",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "CMAKE_CXX_COMPILER": "clang++",
                "CMAKE_CXX_FLAGS": "-Wall -Wextra -Wpedantic -fsanitize=address,undefined",
                "CMAKE_EXE_LINKER_FLAGS": "-fsanitize=address,undefined",
                "CONTUR2_ENABLE_TRACING": "ON"
            }
        },
        {
            "name": "release",
            "displayName": "Release (Clang)",
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build/release",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release",
                "CMAKE_CXX_COMPILER": "clang++",
                "CMAKE_CXX_FLAGS": "-Wall -Wextra -Wpedantic"
            }
        },
        {
            "name": "gcc-debug",
            "displayName": "Debug (GCC)",
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build/gcc-debug",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "CMAKE_CXX_COMPILER": "g++",
                "CMAKE_CXX_FLAGS": "-Wall -Wextra -Wpedantic -fsanitize=address,undefined",
                "CMAKE_EXE_LINKER_FLAGS": "-fsanitize=address,undefined"
            }
        }
    ],
    "buildPresets": [
        { "name": "debug", "configurePreset": "debug" },
        { "name": "release", "configurePreset": "release" },
        { "name": "gcc-debug", "configurePreset": "gcc-debug" }
    ],
    "testPresets": [
        {
            "name": "debug",
            "configurePreset": "debug",
            "output": { "outputOnFailure": true }
        }
    ]
}
```

### Build & Run

```bash
# Configure + Build (Debug, Clang)
cmake --preset debug -S src
cmake --build --preset debug

# Run
./src/build/debug/app/contur2

# Run tests
ctest --preset debug

# Configure + Build (Release, Clang)
cmake --preset release -S src
cmake --build --preset release

# Build with GCC for compatibility check
cmake --preset gcc-debug -S src
cmake --build --preset gcc-debug
```

### Build Script

```bash
#!/usr/bin/env bash
# src/build.sh — convenience wrapper
# Usage: bash src/build.sh [debug|release|gcc-debug] [source_dir]

set -euo pipefail

PRESET="${1:-debug}"
SOURCE_DIR="${2:-$(dirname "$0")}"

cmake --preset "$PRESET" -S "$SOURCE_DIR"
cmake --build --preset "$PRESET"
```

### Doxygen Documentation

API documentation is auto-generated from code comments using Doxygen:

```cmake
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_GENERATE_TREEVIEW YES)
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    doxygen_add_docs(docs
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        COMMENT "Generating API documentation"
    )
endif()
```

```bash
# Generate docs
cmake --build --preset debug --target docs
# Open in browser
open src/build/debug/html/index.html
```

Comment style for Doxygen — use `///` for single-line, `/** ... */` for multi-line:

```cpp
/// @brief Selects the next process to run from the ready queue.
/// @param readyQueue Non-empty vector of PCB pointers (sorted by arrival).
/// @param clock Simulation clock for time-based decisions.
/// @return ProcessId of the selected process.
/// @throw std::logic_error if readyQueue is empty (programming error).
virtual ProcessId selectNext(
    const std::vector<const PCB*>& readyQueue,
    const IClock& clock
) const = 0;
```

### Documentation Style (Doxygen)

Doxygen comments are mandatory for all public APIs in `src/include/contur/`.
Use one consistent style across all subsystems.

#### Required structure for every public header

1. File banner:

```cpp
/// @file scheduler.h
/// @brief Scheduler implementation hosting pluggable scheduling policies.
```

2. Class or interface documentation:

```cpp
/// @brief Scheduler abstraction managing process state queues.
///
/// Coordinates ready/blocked/running process sets and delegates process
/// ordering decisions to the active ISchedulingPolicy implementation.
class IScheduler
```

3. Method-level tags:
- Always include `@brief`
- Include `@param` for each parameter
- Include `@return` for non-void returns
- Use `@pre`, `@post`, `@details`, `@throws` only when behavior is non-obvious

#### Canonical method example

```cpp
/// @brief Selects the process to run next.
/// @param clock Simulation clock for time-aware policy decisions.
/// @return Selected process identifier, or error if no runnable process exists.
[[nodiscard]] virtual Result<ProcessId> selectNext(const IClock& clock) = 0;
```

#### Consistency rules

- Prefer `///` line comments in headers.
- Keep `@brief` concise (one sentence).
- Document API semantics and constraints, not implementation trivia.
- Use parallel wording for similar methods across classes.
- Do not leave public methods undocumented.
- Preserve naming and ownership semantics in docs (`unique_ptr`, non-owning refs, etc.).

#### Scope and exclusions

- Mandatory: all public interfaces/classes/functions in `src/include/contur/**`.
- Optional: private helpers, PIMPL `Impl` internals, and obvious trivial private members.

#### Review checklist before merge

- Header has `@file` and file-level `@brief`.
- Each public class/interface has a `@brief` block.
- Each public method has `@brief`; params/returns are fully tagged.
- Docs match current behavior and error semantics (`Result<T>` paths included).
- Terminology matches project language (process states, ticks, policies, IPC channels, etc.).

---

## Testing Strategy

### Unit Tests
- One test file per class: `test_<class_name>.cpp`
- Test through interfaces where possible
- Use mock implementations for dependencies (hand-written or via a lightweight mock framework)
- Test scheduling policies in isolation with synthetic ready queues
- Test CPU decode/execute with known instruction sequences
- Test MMU address translation and swap operations
- Test priority calculations and dynamic adjustments

### Integration Tests
- `test_dispatcher_flow.cpp` — full process lifecycle through Dispatcher
- `test_interpreter_execution.cpp` — load a program into memory, execute via interpreter, verify results
- `test_kernel_api.cpp` — end-to-end tests through the Kernel public API

### Test Framework
Use **Google Test** (gtest) — widely available, integrates well with CMake's `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)
```

---

## Implementation Order

Recommended implementation sequence (bottom-up, each step builds on the previous):

| Phase | Components | Deliverables |
|---|---|---|
| **Phase 1: Foundation** | `core/` (types, error, clock, event) + `arch/` (instruction, interrupt, register_file, block) | Type system, scoped enums, `Result<T>`, `IClock` |
| **Phase 2: Memory** | `memory/` (IMemory, PhysicalMemory, IMMU, MMU, PageTable) | Working memory simulation with unit tests |
| **Phase 3: Process** | `process/` (state, priority, PCB, ProcessImage) | Process model with composition, priority system |
| **Phase 4: CPU** | `cpu/` (ICPU, CPU, ALU) + `io/` (IDevice, ConsoleDevice, NetworkDevice) | Fetch-decode-execute with I/O |
| **Phase 5: Interpreter** | `execution/` (IExecutionEngine, InterpreterEngine) | Full instruction interpretation |
| **Phase 6: Scheduling** | `scheduling/` (IScheduler, Scheduler, all policies, Statistics) | All 7 scheduling algorithms + page replacement |
| **Phase 7: Dispatch** | `dispatch/` (IDispatcher, Dispatcher, MPDispatcher) + `sync/` (Mutex, Semaphore, DeadlockDetector) | Process lifecycle, synchronization, deadlock detection |
| **Phase 8: IPC & Syscalls** | `ipc/` (Pipe, SharedMemory, MessageQueue) + `syscall/` (SyscallTable) | Inter-process communication, system call dispatch |
| **Phase 9: File System** | `fs/` (IFileSystem, SimpleFS, Inode, BlockAllocator) | Inode-based FS simulation |
| **Phase 10: Kernel** | `kernel/` (IKernel, Kernel, KernelBuilder) | Top-level API, DI wiring |
| **Phase 11: Tracing** | `tracing/` (ITracer, Tracer, NullTracer, TraceScope, sinks) | Hierarchical call tracing, compile-time toggle |
| **Phase 12: TUI** | `tui/` (IRenderer, Dashboard, ProcessView, MemoryMapView, SchedulerView) | Terminal visualization |
| **Phase 13: Demos** | `demos/` (Stepper, all demo functions) + `app/main.cpp` | Interactive CLI; step-by-step in Debug, continuous in Release |
| **Phase 14: Native** | `execution/NativeEngine` | Real process management on host OS |
| **Phase 15: Tests** | `tests/unit/` + `tests/integration/` | Full test coverage |

---

## Status Checklist

- [ ] Foundation types and error handling (`core/`)
- [ ] Architecture enums and register file (`arch/`)
- [ ] Memory subsystem with MMU (`memory/`)
- [ ] Process model with priorities (`process/`)
- [ ] CPU with ALU (`cpu/`)
- [ ] I/O device abstraction (`io/`)
- [ ] Bytecode interpreter engine (`execution/`)
- [ ] Scheduling policies (7 algorithms) (`scheduling/`)
- [ ] Dispatcher and multiprocessor support (`dispatch/`)
- [ ] Synchronization primitives (`sync/`)
- [ ] Deadlock detection & prevention (`sync/deadlock_detector`)
- [ ] IPC: pipes, shared memory, message queues (`ipc/`)
- [ ] System calls interface (`syscall/`)
- [ ] File system simulation (`fs/`)
- [ ] Kernel API and builder (`kernel/`)
- [ ] Tracing subsystem (`tracing/`)
- [ ] Terminal UI / Visualization (`tui/`)
- [ ] Step-by-step demo mode (`demos/stepper.h`)
- [ ] Demo programs (`demos/`)
- [ ] CLI entry point (`app/`)
- [ ] Native execution engine (`execution/`)
- [ ] Unit tests (`tests/unit/`)
- [ ] Integration tests (`tests/integration/`)
- [ ] `.clang-format` and `.clang-tidy` configuration
- [ ] CI pipeline (Clang + GCC build matrix)
