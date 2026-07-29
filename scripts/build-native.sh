#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build/native"

echo "=== iox-cpp native build ==="
echo "Source:  ${PROJECT_DIR}"
echo "Build:   ${BUILD_DIR}"

mkdir -p "$BUILD_DIR"

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    "$@"

cmake --build "$BUILD_DIR" --parallel

echo "=== Native build complete ==="
