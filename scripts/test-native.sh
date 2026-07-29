#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build/native"

echo "=== iox-cpp native tests ==="

if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory not found. Run build-native.sh first."
    exit 1
fi

cd "$BUILD_DIR"
ctest --output-on-failure "$@"

echo "=== Native tests complete ==="
