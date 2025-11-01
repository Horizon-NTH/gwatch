#!/usr/bin/env bash
set -euo pipefail

CONFIG="Release"
TESTS=OFF

while [[ $# -gt 0 ]]; do
  case "$1" in
    -c|--config)
      CONFIG="$2"; shift 2;;
    -t|--tests)
      TESTS=ON; shift;;
    *) echo "Unknown arg: $1"; exit 2;;
  esac
done

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
BUILD_DIR="$ROOT_DIR/build"
mkdir -p "$BUILD_DIR"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DENABLE_TESTS=${TESTS}
cmake --build "$BUILD_DIR" --config "$CONFIG" -j

echo "Build completed. Binaries in: $BUILD_DIR/bin or $BUILD_DIR/$CONFIG"
