#!/usr/bin/env bash
# EQ8 sub-80 Hz host test - Q24 precision
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/eq_sub80_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -I"$SRC" "$ROOT/tests/host/eq_sub80_host_test.cpp" "$SRC/Application/Audio/InstrumentEq.cpp" -lm -o "$TMP/eq_sub80_host_test"
"$TMP/eq_sub80_host_test"
