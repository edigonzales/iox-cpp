#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build/coverage"
PROFILE_DIR="${BUILD_DIR}/profiles"

echo "=== iox-cpp coverage ==="
echo "Requires Clang/LLVM or GCC."

mkdir -p "$BUILD_DIR"

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DIOX_ENABLE_COVERAGE=ON \
    "$@"

cmake --build "$BUILD_DIR" --parallel

mkdir -p "$PROFILE_DIR"
find "$BUILD_DIR" -name '*.profraw' -delete

(
    cd "$BUILD_DIR"
    LLVM_PROFILE_FILE="$PROFILE_DIR/%m-%p.profraw" ctest --output-on-failure
)

if command -v llvm-profdata >/dev/null 2>&1 && command -v llvm-cov >/dev/null 2>&1; then
    LLVM_PROFDATA=llvm-profdata
    LLVM_COV=llvm-cov
elif command -v xcrun >/dev/null 2>&1 \
        && xcrun --find llvm-profdata >/dev/null 2>&1 \
        && xcrun --find llvm-cov >/dev/null 2>&1; then
    LLVM_PROFDATA="xcrun llvm-profdata"
    LLVM_COV="xcrun llvm-cov"
else
    echo "LLVM coverage tools not found; instrumented test gate passed."
    exit 0
fi

shopt -s nullglob
profiles=("$PROFILE_DIR"/*.profraw)
if [ "${#profiles[@]}" -eq 0 ]; then
    echo "No LLVM profile data was produced; instrumented test gate failed."
    exit 1
fi

profile_data="$BUILD_DIR/coverage.profdata"
$LLVM_PROFDATA merge -sparse "${profiles[@]}" -o "$profile_data"

objects=()
while IFS= read -r executable; do
    objects+=("$executable")
done < <(find "$BUILD_DIR" -type f -perm -111 \
    ! -path "$BUILD_DIR/_deps/*" \
    ! -path "$PROFILE_DIR/*" \
    ! -path '*/ilic-core-build/*' \
    ! -name 'iox-test-model-based' | sort)

if [ "${#objects[@]}" -gt 0 ]; then
    first_object="${objects[0]}"
    object_args=()
    for executable in "${objects[@]:1}"; do
        object_args+=( -object "$executable" )
    done
    echo "=== LLVM coverage report (core libraries; third-party, tests, factory, examples, tools, and generated sources excluded) ==="
    report="$($LLVM_COV report "$first_object" "${object_args[@]}" \
        -instr-profile="$profile_data" \
        -ignore-filename-regex='/(build|_deps|test|examples|tools|factory|packages/iox-wasm)/')"
    printf '%s\n' "$report"

    total_line="$(printf '%s\n' "$report" | awk '$1 == "TOTAL" { print; exit }')"
    if [ -z "$total_line" ]; then
        echo "No TOTAL row was produced; coverage threshold gate failed."
        exit 1
    fi
    line_coverage="$(printf '%s\n' "$total_line" | awk '{gsub(/%/, "", $10); print $10}')"
    branch_coverage="$(printf '%s\n' "$total_line" | awk '{gsub(/%/, "", $13); print $13}')"
    echo "Coverage thresholds: line=${line_coverage}% (required >= 90%), branch=${branch_coverage}% (required >= 85%)."
    if ! awk -v value="$line_coverage" 'BEGIN { exit !(value + 0 >= 90.0) }'; then
        echo "Line coverage threshold gate failed."
        exit 1
    fi
    if ! awk -v value="$branch_coverage" 'BEGIN { exit !(value + 0 >= 85.0) }'; then
        echo "Branch coverage threshold gate failed."
        exit 1
    fi
else
    echo "No instrumented project executables found; coverage report unavailable."
    exit 1
fi

echo "=== Coverage build and tests complete ==="
