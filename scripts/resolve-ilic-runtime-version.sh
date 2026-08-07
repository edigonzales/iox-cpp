#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 || ! "$1" =~ ^[0-9a-f]{40}$ ]]; then
    echo "usage: $0 <ilic-commit-sha>" >&2
    exit 2
fi

ilic_sha="$1"
metadata_file="$(mktemp)"
trap 'rm -f "$metadata_file"' EXIT

curl --fail --silent --show-error --location \
    "https://raw.githubusercontent.com/edigonzales/ilic-fork/${ilic_sha}/CMakeLists.txt" \
    > "$metadata_file"

base_version="$(sed -nE 's/^project\(ilic VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' "$metadata_file" | head -n 1)"
qualifier="$(sed -nE 's/^set\(ILIC_VERSION_QUALIFIER "([^"]+)".*/\1/p' "$metadata_file" | head -n 1)"

[[ "$base_version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    echo "Could not read ilic base version at ${ilic_sha}" >&2
    exit 1
}

case "$qualifier" in
    ""|RELEASE)
        printf '%s\n' "$base_version"
        ;;
    *)
        printf '%s-%s\n' "$base_version" "$qualifier"
        ;;
esac
