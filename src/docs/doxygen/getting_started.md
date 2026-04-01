# Getting started

This short guide extracts the essential local development and debugging steps. Use it to get up and running quickly on your platform.

## Overview

This document covers:

- Required developer tools (dependencies)
- How to build the project
- Running tests
- Generating API documentation
- Where build artifacts are written
- Debugging with VSCode + LLDB

## Dependencies

The project requires the following tools to build and test locally:

- `llvm` (includes `clang`) — REQUIRED
- `cmake` — REQUIRED
- `ninja` — REQUIRED
- `git` — REQUIRED (used by CMake to clone/fetch GoogleTest and other externals)
- `doxygen` — optional (for generating API documentation)
- `gcc` — optional (commonly used on Linux/macOS)
- `Python 3.11` — REQUIRED on **Windows only** (required by LLVM for the build on Windows)

Note: Linux and macOS do not require Python for the standard build flow.

### Install examples

Linux (Debian / Ubuntu):

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git llvm
sudo apt install -y doxygen   # optional
```

Arch / Manjaro:

```bash
sudo pacman -Syu
sudo pacman -S --needed base-devel cmake ninja git llvm
sudo pacman -S doxygen        # optional
```

macOS (Homebrew):

```bash
brew update
brew install llvm cmake ninja doxygen
# If installed via Homebrew, add llvm to PATH when needed
# echo 'export PATH="/opt/homebrew/opt/llvm/bin:$PATH"' >> ~/.zshrc
```

Windows (winget):

```powershell
winget install --id LLVM.LLVM -e
winget install --id Kitware.CMake -e
winget install --id Ninja.MSBuild -e
winget install --id Python.Python.3.11 -e
winget install --id Doxygen.Doxygen -e   # optional
```

## Build

Two supported workflows are provided: VSCode tasks (recommended) and CLI scripts.

### VSCode (recommended)

Open the command palette (Ctrl+Shift+P) → `Tasks: Run Task` and choose:

- `Build contur2 (Debug)`
- `Build contur2 (Release)`

These tasks invoke CMake with the `debug`/`release` presets.

### CLI

Linux / macOS (Bash):

```bash
# Debug
bash src/build.sh debug src

# Release
bash src/build.sh release src
```

Windows (PowerShell / pwsh):

```powershell
# Debug
pwsh ./src/build.ps1 debug src

# Release
pwsh ./src/build.ps1 release src
```

## Tests

Tests use CTest / GoogleTest. You can run tests via VSCode tasks or the CLI.

### VSCode (recommended)

Open the command palette (Ctrl+Shift+P) → `Tasks: Run Task` and choose:

- `Run Tests (Debug)`
- `Run Tests (Release)`

These tasks call CTest with the appropriate preset.

### CLI

```bash
# Debug (Linux / macOS)
ctest --preset debug --output-on-failure src

# Release (Linux / macOS)
ctest --preset release --output-on-failure src

# Debug (Windows)
ctest --preset win-debug --output-on-failure ./src

# Release (Windows)
ctest --preset win-release --output-on-failure ./src
```

Tip: rebuild after editing tests so the test binaries are updated before running `ctest`.

## Documentation

You can generate API docs with the provided VSCode tasks or via CLI + CMake target `docs`.

### VSCode (recommended)

- `Generate Doxygen Docs (Debug)`
- `Generate Doxygen Docs (Release)`

### CLI

```bash
# Debug (Linux / macOS)
bash -c "src/build.sh debug src && cmake --build --preset debug --target docs"

# Release (Linux / macOS)
bash -c "src/build.sh release src && cmake --build --preset release --target docs"

# Debug (Windows)
pwsh -c "./src/build.ps1 debug src; cmake --build --preset win-debug --target docs"

# Release (Windows)
pwsh -c "./src/build.ps1 release src; cmake --build --preset win-release --target docs"
```

## Where build artifacts go

Useful (*but not all*) build outputs and generated artifacts are placed under `src/build/<preset>/`, where `<preset>` is `debug` or `release`.

- Application: `src/build/<preset>/app/`
- Demos: `src/build/<preset>/demos/`
- Tests: `src/build/<preset>/tests/`
- Test results / CTest data: `src/build/<preset>/Testing/`
- Generated HTML documentation: `src/build/<preset>/html/` (target `docs`)

## Debugging (VSCode + LLDB)

1. Install the LLDB extension for VSCode:
   https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb
2. The workspace includes a preconfigured `.vscode/launch.json` that uses LLDB and launches the application built from `src/app/main.cpp`. No manual `launch.json` edits are required, enjoy debugging :D!

<div class="section_buttons">

| Home                |
|:--------------------|
| [Home](mainpage.md) |

</div>