#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build/wasm"
PACKAGE_DIR="${PROJECT_DIR}/packages/iox-wasm"

EMSCRIPTEN_VERSION="$(cat "$PROJECT_DIR/.emscripten-version")"

echo "=== iox-cpp WASM build ==="
echo "Emscripten: ${EMSCRIPTEN_VERSION}"

if ! command -v emcmake &> /dev/null; then
    for candidate in "${EMSDK:-}" "${PROJECT_DIR}/../emsdk" "/Users/stefan/sources/emsdk"; do
        if [ -n "$candidate" ] && [ -f "$candidate/emsdk_env.sh" ]; then
            # shellcheck disable=SC1090
            source "$candidate/emsdk_env.sh" >/dev/null
            break
        fi
    done
fi

if ! command -v emcc >/dev/null 2>&1; then
    echo "Emscripten ${EMSCRIPTEN_VERSION} is required; emcc was not found." >&2
    exit 1
fi

actual_version="$(emcc --version | sed -n '1s/.*) \([0-9][0-9.]*\).*/\1/p')"
if [ "$actual_version" != "$EMSCRIPTEN_VERSION" ]; then
    echo "Expected Emscripten ${EMSCRIPTEN_VERSION}, found ${actual_version}." >&2
    exit 1
fi

mkdir -p "$BUILD_DIR"

emcmake cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=OFF \
    -DIOX_BUILD_WASM=ON \
    "$@"

cmake --build "$BUILD_DIR" --parallel

if [ -f "$BUILD_DIR/iox-wasm.mjs" ]; then
    cp "$BUILD_DIR/iox-wasm.mjs" "$PACKAGE_DIR/iox-wasm.mjs"
    if [ -f "$BUILD_DIR/iox-wasm.wasm" ]; then
        cp "$BUILD_DIR/iox-wasm.wasm" "$PACKAGE_DIR/iox-wasm.wasm"
    fi
fi

echo "=== WASM build complete ==="
