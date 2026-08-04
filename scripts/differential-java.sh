#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
expected_commit="1af01d4bf6b675a490b9f5ad44d41723fdfa3c0f"
iox_ili_dir="${IOX_ILI_DIR:-${1:-$project_dir/../iox-ili}}"
cases_file="${IOX_DIFFERENTIAL_CASES:-$project_dir/scripts/differential-java-cases.tsv}"
native_dump="${IOX_CPP_DUMP:-$project_dir/build/native/iox-dump}"
work_dir="$project_dir/build/differential-java"

if [ ! -d "$iox_ili_dir/.git" ]; then
    echo "iox-ili checkout not found: $iox_ili_dir" >&2
    echo "Set IOX_ILI_DIR to a local checkout at $expected_commit." >&2
    exit 2
fi
actual_commit="$(git -C "$iox_ili_dir" rev-parse HEAD)"
if [ "$actual_commit" != "$expected_commit" ]; then
    echo "iox-ili commit mismatch: expected $expected_commit, got $actual_commit" >&2
    exit 2
fi
if [ ! -x "$native_dump" ]; then
    echo "native iox-dump not found: $native_dump" >&2
    echo "Set IOX_CPP_DUMP to an existing native build." >&2
    exit 2
fi
if ! command -v java >/dev/null || ! command -v javac >/dev/null; then
    echo "java and javac are required" >&2
    exit 2
fi

iox_ili_jar="${IOX_ILI_JAR:-}"
if [ -z "$iox_ili_jar" ]; then
    for candidate in "$iox_ili_dir"/build/libs/iox-ili-*.jar; do
        if [ -f "$candidate" ]; then
            iox_ili_jar="$candidate"
            break
        fi
    done
fi
if [ ! -f "$iox_ili_jar" ]; then
    echo "pinned local iox-ili jar not found; set IOX_ILI_JAR" >&2
    echo "This script intentionally does not run Gradle or download dependencies." >&2
    exit 2
fi
jar_commit="$(unzip -p "$iox_ili_jar" ch/interlis/iox_j/Version.properties 2>/dev/null \
    | sed -n 's/^versionCommit=//p' | tr -d '\r')"
if [ "$jar_commit" != "$expected_commit" ]; then
    echo "iox-ili jar is not from the pinned commit: $iox_ili_jar" >&2
    exit 2
fi

classpath="$iox_ili_jar:$iox_ili_dir/lib/*"
if [ -n "${IOX_ILI_CLASSPATH:-}" ]; then
    classpath="$IOX_ILI_CLASSPATH:$classpath"
fi
mkdir -p "$work_dir/classes" "$work_dir/native" "$work_dir/java"
javac -encoding UTF-8 -cp "$classpath" -d "$work_dir/classes" \
    "$project_dir/scripts/java/IoxIliEventDump.java"

case_count=0
while IFS=$'\t' read -r transfer model; do
    if [ -z "$transfer" ] || [[ "$transfer" == \#* ]]; then
        continue
    fi
    case_count=$((case_count + 1))
    native_result="$work_dir/native/$case_count.events"
    java_result="$work_dir/java/$case_count.events"
    "$native_dump" --events "$project_dir/$transfer" 2>/dev/null \
        | "$project_dir/scripts/normalize-native-events.py" > "$native_result"
    model_argument="-"
    if [ "$model" != "-" ]; then
        model_argument="$project_dir/$model"
    fi
    java -cp "$work_dir/classes:$classpath" IoxIliEventDump \
        "$project_dir/$transfer" "$model_argument" > "$java_result"
    if ! diff -u "$java_result" "$native_result"; then
        echo "differential mismatch: $transfer" >&2
        exit 1
    fi
    echo "matched: $transfer"
done < "$cases_file"

if [ "$case_count" -eq 0 ]; then
    echo "no differential cases found in $cases_file" >&2
    exit 2
fi
echo "Java differential gate passed: $case_count cases at $expected_commit"
