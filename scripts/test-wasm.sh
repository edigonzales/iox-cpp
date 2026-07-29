#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PACKAGE_DIR="${PROJECT_DIR}/packages/iox-wasm"

echo "=== iox-cpp WASM tests ==="

if [ ! -d "$PACKAGE_DIR" ]; then
    echo "WASM package directory not found. Nothing to test."
    exit 0
fi

cd "$PACKAGE_DIR"

if [ -f "package.json" ]; then
    node --test test/*.test.mjs "$@"
fi

echo "=== WASM tests complete ==="
