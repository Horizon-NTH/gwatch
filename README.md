# gwatch

[![Release](https://img.shields.io/badge/Release-v0.0-blueviolet)](https://github.com/Horizon-NTH/gwatch/releases)
[![Language](https://img.shields.io/badge/Language-C%2B%2B-0052cf)](https://en.wikipedia.org/wiki/C++)
[![Licence](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![CI Windows](https://github.com/Horizon-NTH/gwatch/actions/workflows/ci-windows.yml/badge.svg)](https://github.com/Horizon-NTH/gwatch/actions/workflows/ci-windows.yml)

Global Variable Watcher is a command-line utility that launches a process and prints to stdout every read and write
access
to a specified global variable (integer type, 4–8 bytes).

## Performance check

To verify the 2× slowdown requirement against the bundled stress debuggee (20,000 accesses), run:

```bash
scripts/perf_check.sh --gwatch <build-dir>/Release/gwatch.exe --target <build-dir>/tests/bin/Release/gwatch_debuggee_stress.exe
```

On Windows you can use the batch wrapper instead:

```cmd
scripts\perf_check.bat --gwatch <build-dir>\Release\gwatch.exe --target <build-dir>\tests\bin\Release\gwatch_debuggee_stress.exe
```

CI invokes the same check after Release builds.
