#!/usr/bin/env bash
set -euo pipefail

matrix="docs/iox-ili-test-porting-matrix.md"
test -f "$matrix"

awk -F '|' '
/^\| `[^-].*\.java` / {
    ticks = $3
    method_count = gsub(/`/, "", ticks) / 2
    status = $4
    gsub(/[ `]/, "", status)
    if (status != "adapted" && status != "deliberate-difference" &&
        status != "out-of-scope") {
        print "invalid matrix status: " status > "/dev/stderr"
        exit 1
    }
    total += method_count
    if (status != "out-of-scope") relevant += method_count
    statuses[status] += method_count
}
END {
    if (total != 229 || relevant != 214) {
        print "unexpected method inventory: total=" total ", relevant=" relevant > "/dev/stderr"
        exit 1
    }
    if (statuses["adapted"] == 0 || statuses["deliberate-difference"] == 0 ||
        statuses["out-of-scope"] == 0) {
        print "matrix must use all three release statuses" > "/dev/stderr"
        exit 1
    }
    print "iox-ili method matrix verified: 214 relevant, 15 out-of-scope"
}' "$matrix"
