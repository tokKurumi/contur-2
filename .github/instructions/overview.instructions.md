# Contur — OS Components Simulator — Copilot Instructions

## Project Overview
Contur is an educational C++ project that models core operating system components: CPU, memory management, process lifecycle, scheduling algorithms, and multiprocessor dispatching. It provides an interactive console menu that demonstrates each subsystem step-by-step, making abstract OS concepts tangible through a working simulation.

## Purpose
This project serves as a teaching aid for understanding:
- Computer architecture (registers, instruction execution)
- Process creation, synchronization (critical sections), and termination
- Memory management (MMU, virtual memory, swap in/out)
- Scheduling algorithms: FCFS, Round Robin, SPN, SRT, HRRN, Dynamic Priority
- Multiprocessor scheduling with global queue assignment

## Architecture

### Directory Structure
```
contur/
├── .github/
│   └── COPILOT_INSTRUCTIONS.md
├── .gitignore
├── .vscode/
│   ├── c_cpp_properties.json
│   ├── launch.json
│   └── tasks.json
└── src/
    ├── CMakeLists.txt
    ├── build.sh
    ├── main.cpp
    ├── include/contur/        # Public headers (declarations only)
    │   ├── architecture_cpu.h
    │   ├── cpu.h
    │   ├── device.h
    │   ├── dispatcher.h
    │   ├── handle.h
    │   ├── kernel.h
    │   ├── lan.h
    │   ├── memory.h
    │   ├── mmu.h
    │   ├── mp_dispatcher.h
    │   ├── process.h
    │   ├── scheduler.h
    │   ├── statistic.h
    │   └── virtual_memory.h
    └── contur/                # Implementations (.cpp)
        └── (mirrors include/contur/ structure)
```

### Code Organization
- **Headers**: `src/include/contur/*.h` — class declarations, enums, constants
- **Implementations**: `src/contur/*.cpp` — method definitions
- **Entry point**: `src/main.cpp` — interactive console menu

### Layer Dependency Graph
```
main.cpp
  ├── Kernel          → Dispatcher → Scheduler → CPU, MMU, Statistic
  ├── MPDispatcher    → Dispatcher (inherits)
  ├── Dispatcher      → VirtualMemory, Scheduler, MMU, CS
  ├── Scheduler       → CPU, MMU, Statistic, Job, ProcessImage
  ├── CPU             → Memory, Device, LAN
  ├── MMU             → Memory, ProcessImage
  ├── VirtualMemory   → ProcessImage
  ├── ProcessImage    → PCB → Process → HANDLE
  └── Memory / Code   → Block (from ArchitectureCPU)
```

## Key Components

### ArchitectureCPU (`architecture_cpu.h`)
Foundation layer defining the simulated hardware:
- **Block** — single memory cell: instruction code, register index, operand
- **Instruction** enum — `Mov, Add, Sub, Mul, Div, Int, Wmem, Rmem, ...`
- **Interrupt** enum — `OK, Error, Exit, Dev, Lan, Div_0, ...`
- **SysReg** — array of 16 registers (`r1..r14, PC, SP`) with inner `Register` class
- **Name** enum — register identifiers
- **Timer** — global simulation clock with static `Tick()` / `getTime()` / `setTime()`
- **VectorParam** — parameter vector for scheduling prediction modes
- **State** enum — process states: `NotRunning, Running, Blocked, Swapped, ExitProcess, New, Ready`

### Memory (`memory.h`)
- **Memory** — linear block array simulating RAM; supports `setCmd()`, `read()`, `clearMemory()`
- **Code** — inherits Memory; represents a program's code image with a fixed size

### HANDLE (`handle.h`)
- **HANDLE** — base class tracking process timing: `Tenter, Tbegin, Tservice, Texec, Tterminate`
- **CS** — critical section object (inherits HANDLE); holds a boolean lock

### Process (`process.h`)
- **Process** — extends HANDLE with user name and state
- **PCB** — extends Process with address, virtual address, SysReg pointer, priority, time slice
- **ProcessImage** — extends PCB with code pointer, status flag; the full in-memory process representation

### CPU (`cpu.h`)
- Fetches instruction from memory at PC address
- Decodes and executes: arithmetic (`Mov, Add, Sub, Mul, Div`), memory I/O (`Wmem, Rmem`), interrupts (`Int`)
- Dispatches device/network output via `Device` and `LAN` objects
- `decode()` is virtual — allows extension

### Device / LAN (`device.h`, `lan.h`)
- Simple I/O peripherals that print data to stdout (simulating hardware output)

### MMU (`mmu.h`)
- **swapIn()** — loads process code into main memory, updates PC with real address
- **swapOut()** — clears process code from main memory
- Address translation: virtual (PCB) → real (Memory)

### VirtualMemory (`virtual_memory.h`)
- Array of `ProcessImage` slots; allocates/frees process images by index
- Tracks which virtual addresses are occupied

### Scheduler (`scheduler.h`)
- **Job** — struct binding address + process ID + ProcessImage for execution
- **Scheduler** — manages 7 state queues (`processQueue[NUMBER_OF_STATE]`)
- Execution modes:
  - Single program: `execute(addr, id, cpu)`
  - Multitasking: `execute(Job*, MMU*)` — round-robin through jobs with timer ticks
- Scheduling features: time-slice preemption, SPN/SRT prediction via `Statistic`, priority sorting
- **Statistic** integration: exponential weighted average prediction (`getTpredict`)

### Statistic (`statistic.h`)
- **Table** — stores observed execution/service times per process
- **Statistic** — `unordered_map<string, vector<Table>>` tracking per-user observations
- Prediction: weighted exponential average with α = 0.8
- Threshold calculation via sigmoid function for preemptive scheduling

### Dispatcher (`dispatcher.h`)
- Orchestrates the full process lifecycle:
  1. Allocates virtual memory → creates ProcessImage
  2. `dispatch()` — moves processes between state queues (NotRunning → Running/Blocked → ExitProcess)
  3. Calls `scheduleProcess()` + `executeProcess()` on each tick
- Supports critical sections, time slicing, priority scheduling
- **Priorityslice** inner class — maps priority levels to time slice values

### MPDispatcher (`mp_dispatcher.h`)
- Extends Dispatcher for multiprocessor simulation
- Distributes jobs across N processors (each with its own Scheduler copy)
- `scheduleProcess()` divides processes by quota across processors
- `executeProcess()` runs each processor's scheduler independently

### Kernel (`kernel.h`)
- High-level API: `CreateProcess()`, `TerminateProcess()`, `EnterCriticalSection()`, `LeaveCriticalSection()`
- Delegates all work to the Dispatcher

## Code Quality Guidelines
- Use `#pragma once` for header guards
- No MSVC-specific extensions (`_int8`, `strcpy_s`, `stdext`, `<tchar.h>`, `<SDKDDKVer.h>`)
- Use `<cstdint>` types (`int8_t`) and `std::string` instead of char buffers
- Use `nullptr` instead of `NULL`
- No external dependencies — pure C++17 standard library only
- Cross-platform: must compile with both GCC and Clang

## Code Style

### Naming Conventions
- **Classes**: `PascalCase` — `ProcessImage`, `VirtualMemory`, `MPDispatcher`
- **Methods**: `camelCase` — `getState()`, `setAddr()`, `clearMemory()`
- **Debug/lifecycle methods**: `PascalCase` — `Debug()`, `DebugQueue()`, `ProcessTime()`, `Tick()`
- **Enums**: `PascalCase` values — `NotRunning`, `ExitProcess`, `TimeExec`
- **Constants**: `constexpr` with `UPPER_SNAKE_CASE` — `NUMBER_OF_REGISTERS`, `SIZE_OF_REGISTER_NAMES`
- **Member variables**: `camelCase`, no prefix — `currBlock`, `timeSlice`, `sysReg`
- **Local variables**: `camelCase` — `addrReal`, `processImage`, `size_`

### Formatting
- **Indentation**: spaces (4 spaces per indent level)
- **Braces**: opening brace on the next line for function/class definitions; same line for `if`/`for`/`while` when block is short
- **Includes**: system headers first (`<iostream>`, `<vector>`), then project headers (`"contur/..."`)
- **Include paths**: always use `"contur/..."` prefix in `#include` directives

### Modern C++ Practices
- **No `using namespace std`** — always use explicit `std::` prefix (`std::cout`, `std::vector`, `std::string`, etc.)
- **`constexpr` over `#define`** for numeric constants
- **`static_cast<T>()`** instead of C-style casts `(T)`
- **`'\n'`** instead of `std::endl` (avoids unnecessary flush)
- **`std::vector`** over raw arrays for dynamically-sized owned collections (e.g., `Memory::heap`, `VirtualMemory::image`, `Scheduler::sysreg`)
- **`std::unique_ptr`** for owning pointers where lifetime is scoped to one class (e.g., `Dispatcher::virtualMemory`, `Dispatcher::cs`)
- **`const`** on all getter methods that don't modify state
- **Pass `std::string` by `const&`**, not by value
- **Value semantics** for small aggregates — prefer flat arrays of objects over arrays of pointers when objects are fixed-size and don't need polymorphism (e.g., `SysReg::register_[]`, `VectorParam::param[]`)

### Class Design
- **Header/implementation split**: declarations in `src/include/contur/*.h`, definitions in `src/contur/*.cpp`
- **Inline trivial getters/setters** in headers; non-trivial methods in `.cpp`
- **Virtual destructors** on base classes used polymorphically (`HANDLE`, `Memory`, `Dispatcher`)
- **`friend` classes** used sparingly for cross-layer access (`Dispatcher`, `Scheduler` are friends of `ProcessImage`)
- **Inheritance** is public; casting between hierarchy levels uses `static_cast`

### Output
- All debug/diagnostic output goes to `std::cout`
- Use `'\n'` for line endings, not `std::endl`
- Debug methods are named `Debug()`, `DebugQueue()`, `DebugMemory()`, `ProcessTime()`, etc.

## Build & Run
```bash
# Build (Debug)
bash src/build.sh debug ./src

# Build (Release)
bash src/build.sh release ./src

# Run
./src/build/Debug/contur
```

The program presents an interactive menu (options 1–8) demonstrating each OS subsystem.

## Status
- ✅ CPU instruction execution and register model
- ✅ Memory management with MMU (swap in/out)
- ✅ Process lifecycle (create, execute, terminate)
- ✅ Critical section synchronization
- ✅ Virtual memory with PCB
- ✅ Scheduling: FCFS, RR, SPN, SRT, HRRN, Dynamic Priority
- ✅ Multiprocessor scheduling
- ✅ CMake + GCC build (cross-platform)
