#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build/coverage"

echo "=== iox-cpp coverage ==="
echo "Requires Clang/LLVM or GCC."

mkdir -p "$BUILD_DIR"

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DIOX_ENABLE_COVERAGE=ON \
    "$@"

cmake --build "$BUILD_DIR" --parallel

cd "$BUILD_DIR"
ctest --output-on-failure

echo "=== Coverage build and tests complete ==="
echo "Use llvm-cov or gcovr to produce reports."
