# gwatch

[![Release](https://img.shields.io/badge/Release-v1.0-blueviolet)](https://github.com/Horizon-NTH/gwatch/releases)
[![Language](https://img.shields.io/badge/Language-C%2B%2B-0052cf)](https://en.wikipedia.org/wiki/C++)
[![Licence](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![CI Windows](https://github.com/Horizon-NTH/gwatch/actions/workflows/ci-windows.yml/badge.svg)](https://github.com/Horizon-NTH/gwatch/actions/workflows/ci-windows.yml)

## Introduction

gwatch is a command-line Global Variable Watcher. It launches a target process and prints every read and write access to a specified global variable (integer type, 4–8 bytes) to stdout.

> [!CAUTION]
> This program is **Windows-only** and has been tested exclusively with the **MSVC** compiler.  
> It relies on the **Windows Debugging API**, specifically hardware data breakpoints (DR0–DR7).

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Execute](#execute)
- [Usage](#usage)
- [Demo Script](#demo-script)
- [Tests](#tests)
- [Profiling](#profiling)
- [How It Works (Debugging)](#how-it-works-debugging)
- [Dependencies](#dependencies)
- [License](#license)

## Features

- Variable size detection via debug symbols.
- Read/write classification with current and previous values.
- Sample debugee and autotest script provided.

## Requirements

- OS: Windows 10/11
- Compiler/Toolchain: Visual Studio 2019/2022 (MSVC) with Windows SDK
- Build system: CMake 3.14+
- Libraries: DbgHelp (bundled with the Windows SDK/Visual Studio)
- Scripts: PowerShell 5+ (Windows PowerShell) or PowerShell 7+ (pwsh)
- Optional: GoogleTest (fetched automatically when `-DENABLE_TESTS=ON`)

## Installation

### Get Source Code

```bash
git clone https://github.com/Horizon-NTH/gwatch.git
cd gwatch
```

### Build

> [!TIP]
> You can directly run the [demo-script](#demo-script), it will build and automatically test the program on some examples.

With CMake directly:

```bash
mkdir build && cd build
cmake .. [-DENABLE_TESTS=ON]
cmake --build . --config Release -j
```

Or use the provided build scripts:

```powershell
scripts\build.ps1 -Config Release [-Tests]
```

```bash
scripts/build.sh --config Release [--tests]
```

Artifacts are written to `build/bin` or `build/<Config>` depending on the generator.

## Execute

```bash
gwatch --var <symbol> --exec <path> [-- arg1 ... argN]
```

Example with the bundled debugee (increments `g_counter` 4 times):

```powershell
> build\bin\gwatch.exe --var g_counter --exec build\tests\bin\gwatch_debuggee_app.exe
g_counter read 0
g_counter write 0 -> 1
g_counter read 1
g_counter write 1 -> 2
g_counter read 2
g_counter write 2 -> 3
g_counter read 3
g_counter write 3 -> 4
```

## Usage

```bash
gwatch [--help | -h]
gwatch --var <symbol> --exec <path> [-- arg1 ... argN]
```

Notes:
- `--var` is the global variable name (4–8 byte integer).
- `--exec` is the target executable path.
- Use `--` to separate watcher options from target args.
- Errors are printed to stderr and return a nonzero code (e.g., symbol not found, unsupported type).

## Demo Script

To build, run tests, see execution with a sample debugee, and then measure stress performance once without and once with gwatch:

```powershell
scripts\demo.ps1 -Config Release
```

```bash
scripts/demo.sh --config Release --iters 20000
```

## Tests

Unit tests (GTest) can be enabled with `-DENABLE_TESTS=ON`. 
Run via your CTest integration or the generated `runTests` binary in `build/tests/bin`.

## Profiling

An optional internal profiler is available to understand where time is spent. It is disabled by default and prints a short summary to stderr when the program exits.

Enable it at configure time and rebuild:

```bash
cmake -S . -B build -DENABLE_TESTS=ON -DGWATCH_PROFILE=ON
cmake --build build --config Release -j
```

Run as usual and the profiling result will be redirected to stderr:

```powershell
> build\bin\gwatch.exe --var g_counter --exec build\tests\bin\gwatch_debuggee_app.exe
g_counter read 0
g_counter write 0 -> 1
g_counter read 1
g_counter write 1 -> 2
g_counter read 2
g_counter write 2 -> 3
g_counter read 3
g_counter write 3 -> 4
[profiling] program total: 16.923 ms
[profiling] launch total=5.892 ms
[profiling] resolve total=2.770 ms
[profiling] setup total=0.022 ms
[profiling] debug loop: iters=24 wait_total=6.694 ms handle_total=4.190 ms handle_avg=0.175 ms
[profiling] events: 8 total=0.504 ms avg=0.063 ms
[profiling] read_value calls: 9 total=0.025 ms avg=2.744 us
[profiling] logger calls: 8 total=0.475 ms avg=59.438 us
[profiling] other handler time total=0.004 ms
[profiling] loop non-sink overhead total=3.686 ms
```

The report includes totals/averages for launch/symbol resolution/setup, debug loop wait/handle time, per-event handler timing, read_value time, and logging time. 
Profiling output goes to stderr so it never mixes with the required stdout access log.

## How It Works (Debugging)

- Launch: The target is started under the Windows Debugging API (`DEBUG_ONLY_THIS_PROCESS`).
- Symbol resolution: On the initial create‑process event, DbgHelp (`SymInitialize`, `SymFromName`, `SymGetTypeInfo` with `TI_GET_LENGTH`) resolves the global’s address and size (must be 4 or 8 bytes).
- Watchpoints: For each thread, a hardware data breakpoint is set in DR0 and enabled in DR7 (local enable). RW is configured to read/write, LEN matches 4 or 8 bytes and DR6 is cleared.
- Handling: Each read/write triggers `EXCEPTION_SINGLE_STEP`. The handler reads the current value (`ReadProcessMemory`) and compares with the last value to classify: changed → `write old -> new`, unchanged → `read value`.
- Threads: New threads are armed with the same watchpoint.

### Performance note:

> [!WARNING] 
> The program does not meet the requirement of running at less than 2× the baseline speed of the target process.

The main performance bottleneck comes from the operating system itself: every read or write access to the watched variable triggers a hardware data breakpoint, which in turn generates an exception.
Each exception requires a round-trip through the debugger (via `WaitForDebugEvent` and `ContinueDebugEvent`), and the cumulative overhead from the OS far outweighs the runtime of the target program without debugging.

A different approach would likely be required to achieve significantly better performance.

## Dependencies

- Windows Debugging API + DbgHelp.
- GoogleTest.

## License

This project is licensed under the [MIT License](LICENSE).
