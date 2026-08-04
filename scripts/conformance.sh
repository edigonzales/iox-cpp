#!/usr/bin/env bash
set -euo pipefail

# Backward-compatible entry point for the optional offline Java comparison.
script_dir="$(cd "$(dirname "$0")" && pwd)"
exec "$script_dir/differential-java.sh" "$@"
