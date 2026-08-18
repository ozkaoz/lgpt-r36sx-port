#!/usr/bin/env bash
# EQ8_STRUCT (bacon-1.5, item 4): host DSP test of the InstrumentEq
# structural rework (per-band-per-channel states + exact smoothing).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/eq8_struct_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/eq8_struct_host_test.cpp" \
  "$SRC/Application/Audio/InstrumentEq.cpp" \
  -lm \
  -o "$TMP/eq8_struct_host_test"
"$TMP/eq8_struct_host_test"