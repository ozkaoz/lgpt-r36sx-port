#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/sp404_perf.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
g++ -std=c++17 -g -O2 -Wall -Wextra -I"$SRC" "$ROOT/tests/host/sp404_performance_bench_host_test.cpp" -o "$TMP/test"
"$TMP/test"
