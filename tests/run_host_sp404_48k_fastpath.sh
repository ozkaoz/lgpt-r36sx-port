#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/sp404_fastpath.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
g++ -std=c++17 -g -Wall -Wextra -I"$SRC" "$ROOT/tests/host/sp404_48k_fastpath_bitexact_host_test.cpp" -o "$TMP/test"
"$TMP/test"
