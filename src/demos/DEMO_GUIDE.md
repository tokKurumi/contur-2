# Demos Guide

This document defines shared requirements for lab demos.

## General Rules

- Each demo lives in its own folder inside the demos directory.
- Demo folders must not be named after lab numbers.
- Each demo must be interactive (TUI or CLI with user input).
- Each demo must include a minimal executable that solves the lab task.
- Each demo folder must include a README describing the goal, steps, and results.

## Demo Scope: Business Logic Only

**Demos must contain only business logic** — the specific processes, data flow, and
configuration that illustrate the lab scenario.

If you find yourself writing something that could be reused across demos or that does not
describe the *what* of the scenario (only the *how*), it belongs in the kernel library, not
in the demo:

| Belongs in the **kernel library** | Belongs in the **demo** |
|---|---|
| Block construction helpers (`arch/program_builder.h`) | Process programs (`makeP1Program`, `makeP2Program`) |
| Syscall sequence emitters (`syscall/program_syscalls.h`) | Kernel build setup for the specific scenario |
| File value I/O utilities (`fs/fs_utils.h`) | Resource registration (which files/sockets, initial values) |
| Generic string parsing helpers | User input prompts and CLI/TUI wiring |
| Infrastructure patterns used by ≥ 2 demos | Domain constants (shared-memory addresses, flag values) |

**Rule of thumb:** if removing the code from the demo and putting it into the kernel would
make the demo *simpler without losing scenario clarity*, it belongs in the kernel.

## Demo Folder Structure

Example structure for one demo folder:

- CMakeLists.txt
- README.md
- src/
  - main.cpp
- include/ (optional)

## Build Integration

- Each demo is added as a separate CMake subdirectory.
- Use the contur2_add_demo function to create the demo executable.
- Target names must be unique across the project.

## Demo README

Each README must include:

- Demo name and short description.
- Which lab it illustrates (without using lab numbers).
- User interaction scenario (keys, commands, steps).
- Results: what the demo shows and what conclusions can be drawn.
