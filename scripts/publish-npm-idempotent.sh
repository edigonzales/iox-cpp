#!/usr/bin/env bash

set -euo pipefail

: "${TARBALL:?TARBALL is required}"
: "${PACKAGE_NAME:?PACKAGE_NAME is required}"
: "${EXPECTED_VERSION:?EXPECTED_VERSION is required}"
: "${EXPECTED_SHA:?EXPECTED_SHA is required}"
: "${NPM_TAG:?NPM_TAG is required}"

[[ -f "$TARBALL" ]] || { echo "Tarball does not exist: $TARBALL" >&2; exit 1; }
[[ "$EXPECTED_SHA" =~ ^[0-9a-f]{40}$ ]] || {
  echo "EXPECTED_SHA must be a full lowercase Git SHA" >&2
  exit 1
}

verify_attempts=${NPM_VERIFY_ATTEMPTS:-12}
verify_delay_seconds=${NPM_VERIFY_DELAY_SECONDS:-5}
[[ "$verify_attempts" =~ ^[1-9][0-9]*$ ]] || {
  echo "NPM_VERIFY_ATTEMPTS must be a positive integer" >&2
  exit 1
}
[[ "$verify_delay_seconds" =~ ^[0-9]+$ ]] || {
  echo "NPM_VERIFY_DELAY_SECONDS must be a non-negative integer" >&2
  exit 1
}

local_integrity=$(node -e '
  const { createHash } = require("node:crypto");
  const { readFileSync } = require("node:fs");
  process.stdout.write(`sha512-${createHash("sha512").update(readFileSync(process.argv[1])).digest("base64")}`);
' "$TARBALL")

lookup_published() {
  local metadata actual_version actual_sha actual_integrity
  if ! metadata=$(npm view "$PACKAGE_NAME@$EXPECTED_VERSION" \
      version gitHead dist.integrity --json --prefer-online --userconfig=/dev/null 2>/dev/null); then
    return 1
  fi

  actual_version=$(jq -r '.version // empty' <<<"$metadata")
  actual_sha=$(jq -r '.gitHead // empty' <<<"$metadata")
  actual_integrity=$(jq -r '."dist.integrity" // empty' <<<"$metadata")
  if [[ "$actual_version" != "$EXPECTED_VERSION" ||
        "$actual_sha" != "$EXPECTED_SHA" ||
        "$actual_integrity" != "$local_integrity" ]]; then
    {
      echo "Published npm metadata conflicts with the immutable local artifact"
      echo "  expected: version=$EXPECTED_VERSION gitHead=$EXPECTED_SHA integrity=$local_integrity"
      echo "  actual:   version=$actual_version gitHead=$actual_sha integrity=$actual_integrity"
    } >&2
    return 2
  fi
}

verify_published() {
  local attempts=$1 attempt status
  for ((attempt = 1; attempt <= attempts; attempt++)); do
    if lookup_published; then
      return 0
    else
      status=$?
    fi
    if [[ "$status" -eq 2 ]]; then
      return 2
    fi
    if [[ "$attempt" -lt "$attempts" ]]; then
      sleep "$verify_delay_seconds"
    fi
  done
  return 1
}

if verify_published 1; then
  echo "Already published identical $PACKAGE_NAME@$EXPECTED_VERSION from $EXPECTED_SHA"
  exit 0
else
  initial_status=$?
fi
if [[ "$initial_status" -eq 2 ]]; then
  exit 1
fi

publish_status=0
if npm publish "$TARBALL" --access public --tag "$NPM_TAG"; then
  publish_status=0
else
  publish_status=$?
  echo "npm publish failed; checking for an identical version that became visible concurrently" >&2
fi

if verify_published "$verify_attempts"; then
  if [[ "$publish_status" -ne 0 ]]; then
    echo "The failed publish was an idempotent duplicate of the now-visible package"
  fi
  exit 0
else
  verify_status=$?
fi
if [[ "$verify_status" -eq 2 ]]; then
  exit 1
fi
if [[ "$publish_status" -ne 0 ]]; then
  exit "$publish_status"
fi
echo "Published package did not become visible within the verification window" >&2
exit 1
