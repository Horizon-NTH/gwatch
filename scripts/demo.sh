#!/usr/bin/env bash
set -Eeuo pipefail
shopt -s inherit_errexit || true

# ---------- traps & log ----------
trap 'echo; echo "[ERROR] Command failed (line $LINENO): $BASH_COMMAND" >&2' ERR

log()  { echo "$@"; }
die()  { echo "Error: $*" >&2; exit 1; }

CONFIG="Release"
VERBOSE=0

usage() {
  cat <<EOF
Usage: $0 [options]
  -c, --config   Debug|Release|RelWithDebInfo|MinSizeRel (default: Release)
  -v, --verbose  Verbose (montre toute la sortie et active xtrace)
  -h, --help     Show help
EOF
}

while (($#)); do
  case "$1" in
    -c|--config) [[ $# -ge 2 ]] || die "Missing value for $1"; CONFIG="$2"; shift 2;;
    -v|--verbose) VERBOSE=1; shift;;
    -h|--help) usage; exit 0;;
    *) die "Unknown argument: $1";;
  esac
done

case "$CONFIG" in
  Debug|Release|RelWithDebInfo|MinSizeRel) ;;
  *) die "Invalid --config '$CONFIG'";;
esac

[[ $VERBOSE -eq 1 ]] && set -x

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT/build"
TEST_DIR="$ROOT/build/tests"

exec_maybe_quiet() {
  if [[ $VERBOSE -eq 1 ]]; then "$@"; else "$@" ; fi
}

resolve_bin_path() {
  local bd="$1" name="$2"
  local candidates=(
    "$bd/bin/${name}.exe"
    "$bd/$CONFIG/${name}.exe"
    "$bd/bin/${name}"
    "$bd/$CONFIG/${name}"
  )
  for p in "${candidates[@]}"; do
    if [[ -x "$p" || -f "$p" ]]; then
      echo "$p"; return 0
    fi
  done
  return 1
}

measure_seconds() {
  local t
  TIMEFORMAT=%R
  t=$({ time "$@" >/dev/null; } 2>&1) || true
  printf "%s" "$t"
}

build_project() {
  log "==> Building (with tests)"
  if [[ -x "$SCRIPT_DIR/build.sh" ]]; then
    log "    • Using scripts/build.sh (-c $CONFIG -t)"
    exec_maybe_quiet "$SCRIPT_DIR/build.sh" -c "$CONFIG" -t
    return
  fi
  if [[ -f "$SCRIPT_DIR/build.ps1" ]]; then
    if command -v pwsh >/dev/null 2>&1; then
      log "    • Using scripts/build.ps1 via pwsh"
      exec_maybe_quiet pwsh -NoProfile -File "$SCRIPT_DIR/build.ps1" -Config "$CONFIG" -Tests
      return
    elif command -v powershell >/dev/null 2>&1; then
      log "    • Using scripts/build.ps1 via Windows PowerShell"
      exec_maybe_quiet powershell -NoProfile -ExecutionPolicy Bypass -File "$SCRIPT_DIR/build.ps1" -Config "$CONFIG" -Tests
      return
    else
      die "build.ps1 found but no pwsh/powershell available"
    fi
  fi
  command -v cmake >/dev/null 2>&1 || die "cmake not found"
  log "    • Using CMake fallback"
  exec_maybe_quiet cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG"
  exec_maybe_quiet cmake --build "$BUILD_DIR" --config "$CONFIG" -j
}

run_tests() {
  log "==> Running tests"
  pushd "$BUILD_DIR" >/dev/null
  local runTestsBin=""
  if runTestsBin="$(resolve_bin_path "$TEST_DIR" "runTests")"; then
    log "    • Running $runTestsBin"
    "$runTestsBin"
  elif command -v ctest >/dev/null 2>&1; then
    if [[ -d "$TEST_DIR" ]]; then
      log "    • Running ctest in $TEST_DIR"
      ctest --test-dir "$TEST_DIR" -C "$CONFIG" --output-on-failure
    else
      log "    • Running ctest in $BUILD_DIR"
      ctest -C "$CONFIG" --output-on-failure
    fi
  else
    popd >/dev/null
    die "runTests not found and ctest unavailable"
  fi
  popd >/dev/null
}

# --- Build + tests ---
build_project
run_tests

# --- Resolve binaries ---
gwatch="$(resolve_bin_path "$BUILD_DIR" "gwatch")" || die "gwatch binary not found in $BUILD_DIR"
debugee="$(resolve_bin_path "$TEST_DIR" "gwatch_debuggee_app")" || die "sample debugee not found in $TEST_DIR"
stress="$(resolve_bin_path "$TEST_DIR" "gwatch_debuggee_stress")" || die "stress test debugee not found in $TEST_DIR"

# --- Show sample source ---
log "==> Showing sample debugee source (tests/debugee/app.cpp)"
if [[ -f "$ROOT/tests/debugee/app.cpp" ]]; then
  cat "$ROOT/tests/debugee/app.cpp"
else
  log "(missing: $ROOT/tests/debugee/app.cpp)"
fi

# --- Quick demo ---
log "==> Quick demo of gwatch"
"$gwatch" --var g_counter --exec "$debugee"

# --- Stress bench ---
log "==> Stress timing: bare (20000 iterations)"
t_bare="$(measure_seconds "$stress")"
printf "   bare:   %.3f s\n" "${t_bare}"

log "==> Stress timing: gwatch + stress test (20000 iterations)"
t_watch="$(measure_seconds "$gwatch" --var g_counter --exec "$stress")"
printf "   gwatch: %.3f s\n" "${t_watch}"

ratio="$(awk -v w="${t_watch:-0}" -v b="${t_bare:-0}" 'BEGIN{ if (b>0) printf "%.2f", w/b; else print "nan"; }')"
printf "   ratio:  %sx\n" "$ratio"

if [[ $VERBOSE -eq 1 ]]; then
  log "==> Env"
  uname -a || true
  echo "bash: $(bash --version | head -n1)"
  command -v cmake  >/dev/null 2>&1 && cmake --version  | head -n1 || true
  command -v ctest  >/dev/null 2>&1 && ctest --version  | head -n1 || true
  command -v ninja  >/dev/null 2>&1 && ninja --version  | head -n1 || true
  command -v pwsh   >/dev/null 2>&1 && pwsh --version   | head -n1 || true
  command -v gcc    >/dev/null 2>&1 && gcc --version    | head -n1 || true
  command -v clang  >/dev/null 2>&1 && clang --version  | head -n1 || true
fi
