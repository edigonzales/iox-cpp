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

mkdir -p "$PROFILE_DIR"
find "$BUILD_DIR" -name '*.profraw' -delete

cmake --build "$BUILD_DIR" --target iox-test-coverage-all --parallel

LLVM_PROFILE_FILE="$PROFILE_DIR/coverage-%m-%p.profraw" \
    "$BUILD_DIR/test/iox-test-coverage-all"

compiler_id="$(sed -n 's/^set(CMAKE_CXX_COMPILER_ID "\([^"]*\)")/\1/p' \
    "$BUILD_DIR/CMakeFiles/"*/CMakeCXXCompiler.cmake | head -n 1)"

if [ "$compiler_id" = "GNU" ]; then
    if ! command -v gcovr >/dev/null 2>&1; then
        echo "GCC coverage requires gcovr." >&2
        exit 2
    fi
    coverage_failed=0
    modules=(core json xml xtf abi)
    line_thresholds=(90 90 90 90 90)
    branch_thresholds=(85 85 85 85 85)
    if grep -q '^IOX_ENABLE_ILIC:BOOL=ON$' "$BUILD_DIR/CMakeCache.txt"; then
        modules+=(ilic)
        line_thresholds+=(85)
        branch_thresholds+=(75)
    fi
    for index in "${!modules[@]}"; do
        module="${modules[$index]}"
        echo "=== iox-$module ==="
        if ! gcovr --root "$PROJECT_DIR" \
            --filter "$PROJECT_DIR/source/$module/" \
            --exclude "$PROJECT_DIR/build" --txt \
            --fail-under-line "${line_thresholds[$index]}" \
            --fail-under-branch "${branch_thresholds[$index]}"; then
            coverage_failed=1
        fi
    done
    gcovr --root "$PROJECT_DIR" --filter "$PROJECT_DIR/source/" \
        --exclude "$PROJECT_DIR/build" --html-details \
        --html "$BUILD_DIR/coverage.html"
    if [ "$coverage_failed" -ne 0 ]; then
        echo "One or more per-module coverage thresholds failed." >&2
        exit 1
    fi
    echo "=== Coverage build and tests complete ==="
    exit 0
elif command -v llvm-profdata >/dev/null 2>&1 && command -v llvm-cov >/dev/null 2>&1; then
    LLVM_PROFDATA=(llvm-profdata)
    LLVM_COV=(llvm-cov)
elif command -v xcrun >/dev/null 2>&1 \
        && xcrun --find llvm-profdata >/dev/null 2>&1 \
        && xcrun --find llvm-cov >/dev/null 2>&1; then
    LLVM_PROFDATA=(xcrun llvm-profdata)
    LLVM_COV=(xcrun llvm-cov)
else
    echo "Neither LLVM coverage tools nor gcovr were found." >&2
    exit 2
fi

shopt -s nullglob
profiles=("$PROFILE_DIR"/*.profraw)
if [ "${#profiles[@]}" -eq 0 ]; then
    echo "No LLVM profile data was produced; instrumented test gate failed."
    exit 1
fi

profile_data="$BUILD_DIR/coverage.profdata"
"${LLVM_PROFDATA[@]}" merge -sparse "${profiles[@]}" -o "$profile_data"

coverage_binary="$BUILD_DIR/test/iox-test-coverage-all"
summary="$BUILD_DIR/coverage-summary.txt"
warnings="$BUILD_DIR/coverage-warnings.txt"
: > "$summary"
: > "$warnings"

report_module() {
    local module="$1"
    local line_required="$2"
    local branch_required="$3"
    shift 3
    local report
    report="$("${LLVM_COV[@]}" report "$coverage_binary" \
        -instr-profile="$profile_data" "$@" 2>>"$warnings")"
    printf '=== %s ===\n%s\n' "$module" "$report" | tee -a "$summary"
    local total_line
    total_line="$(printf '%s\n' "$report" | awk '$1 == "TOTAL" { print; exit }')"
    if [ -z "$total_line" ]; then
        echo "No TOTAL row for $module." >&2
        return 1
    fi
    local line_coverage branch_coverage
    line_coverage="$(printf '%s\n' "$total_line" | awk '{gsub(/%/, "", $10); print $10}')"
    branch_coverage="$(printf '%s\n' "$total_line" | awk '{gsub(/%/, "", $13); print $13}')"
    echo "$module thresholds: line=${line_coverage}% (>=${line_required}%), branch=${branch_coverage}% (>=${branch_required}%)." | tee -a "$summary"
    awk -v value="$line_coverage" -v required="$line_required" \
        'BEGIN { exit !(value + 0 >= required + 0) }' || return 1
    awk -v value="$branch_coverage" -v required="$branch_required" \
        'BEGIN { exit !(value + 0 >= required + 0) }' || return 1
}

coverage_failed=0
for module in core json xml xtf abi; do
    sources=()
    while IFS= read -r source; do
        sources+=("$source")
    done < <(find "$PROJECT_DIR/source/$module" -type f -name '*.cpp' | sort)
    if ! report_module "iox-$module" 90 85 "${sources[@]}"; then
        coverage_failed=1
    fi
done
if [ -d "$PROJECT_DIR/source/ilic" ] && \
   grep -q '^IOX_ENABLE_ILIC:BOOL=ON$' "$BUILD_DIR/CMakeCache.txt"; then
    sources=()
    while IFS= read -r source; do
        sources+=("$source")
    done < <(find "$PROJECT_DIR/source/ilic" -type f -name '*.cpp' | sort)
    if ! report_module "iox-ilic" 85 75 "${sources[@]}"; then
        coverage_failed=1
    fi
fi

if grep -qi 'mismatched data' "$warnings"; then
    cat "$warnings" >&2
    echo "LLVM coverage reported mismatched data." >&2
    exit 1
fi
if [ -s "$warnings" ]; then
    cat "$warnings" >&2
fi

"${LLVM_COV[@]}" show "$coverage_binary" -instr-profile="$profile_data" \
    -format=html -output-dir="$BUILD_DIR/coverage-html" \
    "$PROJECT_DIR/source" >/dev/null

if [ "$coverage_failed" -ne 0 ]; then
    echo "One or more per-module coverage thresholds failed." >&2
    exit 1
fi

echo "=== Coverage build and tests complete ==="
