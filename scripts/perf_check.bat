@echo off
setlocal enabledelayedexpansion

set RUNS=3
set WARMUP=1
set LIMIT=2.0
set GWATCH=
set TARGET=

:parse_args
if "%~1"=="" goto :args_done
if "%~1"=="--gwatch" (
    if "%~2"=="" goto :usage
    set "GWATCH=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="--target" (
    if "%~2"=="" goto :usage
    set "TARGET=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="--runs" (
    if "%~2"=="" goto :usage
    set "RUNS=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="--warmup" (
    if "%~2"=="" goto :usage
    set "WARMUP=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="--ratio-limit" (
    if "%~2"=="" goto :usage
    set "LIMIT=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="--help" goto :usage

echo Unknown argument: %~1 >&2
exit /b 2

:args_done
if "%GWATCH%"=="" goto :usage
if "%TARGET%"=="" goto :usage

if not exist "%GWATCH%" (
    echo gwatch not found: %GWATCH% >&2
    exit /b 2
)
if not exist "%TARGET%" (
    echo target not found: %TARGET% >&2
    exit /b 2
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop';" ^
  "$gwatch = (Get-Item -LiteralPath '%GWATCH%').FullName;" ^
  "$target = (Get-Item -LiteralPath '%TARGET%').FullName;" ^
  "$runs = [int]%RUNS%;" ^
  "$warmup = [int]%WARMUP%;" ^
  "$limit = [double]%LIMIT%;" ^
  "function Invoke-CommandChecked([string[]]$cmd) {" ^
  "    if ($cmd.Length -gt 1) { & $cmd[0] @($cmd[1..($cmd.Length-1)]) | Out-Null } else { & $cmd[0] | Out-Null }" ^
  "    if ($LASTEXITCODE -ne 0) { throw \"Command failed: $($cmd -join ' ')\" }" ^
  "}" ^
  "function Measure-Average([string[]]$cmd) {" ^
  "    for ($i=0; $i -lt $warmup; $i++) { Invoke-CommandChecked $cmd }" ^
  "    $total = 0.0;" ^
  "    for ($i=0; $i -lt $runs; $i++) {" ^
  "        $elapsed = (Measure-Command { Invoke-CommandChecked $cmd }).TotalSeconds;" ^
  "        $total += $elapsed;" ^
  "    }" ^
  "    return $total / [double]$runs" ^
  "}" ^
  "$direct = Measure-Average(@($target));" ^
  "$wrapped = Measure-Average(@($gwatch, '--var', 'g_counter', '--exec', $target));" ^
  "$ratio = if ($direct -gt 0) { $wrapped / $direct } else { [double]::PositiveInfinity };" ^
  "Write-Output ('Direct run avg:  {0:F6} s' -f $direct);" ^
  "Write-Output ('gwatch run avg: {0:F6} s' -f $wrapped);" ^
  "Write-Output ('Slowdown ratio: {0:F3}x (limit {1:F2}x)' -f $ratio, $limit);" ^
  "if ($ratio -le $limit) { exit 0 } else { Write-Error 'ERROR: gwatch exceeds allowed slowdown.'; exit 1 }"
exit /b %ERRORLEVEL%

:usage
echo Usage: perf_check.bat --gwatch ^<path^> --target ^<path^> [--runs N] [--warmup N] [--ratio-limit X]
exit /b 2
