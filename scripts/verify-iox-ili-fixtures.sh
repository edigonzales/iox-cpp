#!/usr/bin/env bash
set -euo pipefail

manifest="test/fixtures/iox-ili-fixtures.tsv"
expected_commit="1af01d4bf6b675a490b9f5ad44d41723fdfa3c0f"

test -f "$manifest"

awk -F '\t' -v expected_commit="$expected_commit" '
NR == 1 {
    if ($1 != "# source_commit" || $2 != "source_path" || $3 != "local_path") {
        print "invalid fixture manifest header" > "/dev/stderr"
        exit 1
    }
    next
}
{
    if ($1 != expected_commit) {
        print "fixture row has an unexpected source commit: " $2 > "/dev/stderr"
        exit 1
    }
    if ($4 != "MIT/X License") {
        print "fixture row has an unexpected license: " $2 > "/dev/stderr"
        exit 1
    }
    if (system("test -f \"" $3 "\"") != 0) {
        print "missing local fixture: " $3 > "/dev/stderr"
        exit 1
    }
    if ($5 == "model-input") models++
    else transfers++
    rows++
}
END {
    if (rows != 220 || transfers != 211 || models != 9) {
        print "unexpected fixture counts: rows=" rows ", transfers=" transfers ", models=" models > "/dev/stderr"
        exit 1
    }
    print "iox-ili fixture manifest verified: 211 transfers, 9 model files"
}' "$manifest"
