# Contur 2 — OS Kernel Simulator

## Overview

Contur 2 is a ground-up rewrite of the Contur educational OS simulator. It models a **real OS kernel** capable of two execution modes:

- **Interpreted mode** — an internal bytecode interpreter emulates x86-like programs step-by-step (educational, fully portable)
- **Native mode** — the kernel manages real child processes on the host OS via platform abstractions (advanced, not ready yet)

## Key Features

- **Educational**: Fully featured OS kernel simulator with hierarchical tracing
- **Modular**: Clean architecture with dependency injection and interfaces
- **Dual-Mode Execution**: Bytecode interpreter + native process management
- **7 Scheduling Algorithms**: FCFS, Round Robin, SPN, SRT, HRRN, Priority, MLFQ
- **Memory Management**: Virtual memory, paging, 4 page replacement algorithms
- **Synchronization**: Mutexes, semaphores, deadlock detection
- **IPC**: Pipes, shared memory, message queues
- **File System**: Inode-based filesystem simulation
- **Terminal UI**: Real-time ANSI-based dashboard visualization (not ready yet)
- **Tracing**: Hierarchical call tracing with compile-time control (not ready yet)

## Architecture

The kernel consists of several interconnected subsystems:

### Core Layer
- **types.h** — Central type definitions
- **error.h** — Error handling (Result<T> pattern)
- **clock.h** — Simulation time source
- **event.h** — Observer pattern infrastructure

### Hardware Abstraction
- **arch/** — Instruction set, interrupts, registers
- **cpu/** — CPU simulation with fetch-decode-execute
- **memory/** — Physical/virtual memory, MMU, paging

### Process Management
- **process/** — Process model with priorities
- **scheduling/** — 7 pluggable scheduling policies
- **dispatch/** — Process dispatcher (uniprocessor + multiprocessor)

### Advanced Features
- **sync/** — Synchronization primitives + deadlock detection
- **ipc/** — Inter-process communication channels
- **syscall/** — System call interface
- **fs/** — File system simulation
- **io/** — I/O device abstraction

### Developer Tools
- **tracing/** — Hierarchical call tracing (Debug-only) (not ready yet)
- **tui/** — Terminal UI with real-time visualization (not ready yet)

## Design Patterns

- **PIMPL** — Compilation firewall & ABI stability
- **Strategy** — Pluggable scheduling algorithms
- **Factory** — KernelBuilder dependency injection
- **Observer** — Event-driven subsystem communication
- **Dependency Inversion** — All cross-layer dependencies use interfaces

## Code Style

- **Language**: C++20
- **Standard**: Clang (primary), GCC (secondary)
- **Formatting**: `.clang-format` (Allman style)
- **Linting**: `.clang-tidy` (strict checks)
- **Memory**: No raw `new`/`delete` — `std::unique_ptr`/`std::shared_ptr` everywhere

## Resources

- **GitHub**: https://github.com/tokKurumi/contur
- **Issue Tracker**: https://github.com/tokKurumi/contur/issues

## License

See [LICENSE](https://github.com/tokKurumi/contur-2/blob/main/LICENSE) file in the repository.

<div class="section_buttons">

|                       Getting started |
|--------------------------------------:|
| [Getting started](getting_started.md) |

</div>
