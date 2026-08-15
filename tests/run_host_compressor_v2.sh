#!/usr/bin/env bash
# FXP_COMPRESSOR_V2 (bacon-1.5, item 4): host test of the Compressor V2
# (sidechain TRACK/BUS, SC HPF, SC AMOUNT, dry/wet MIX) under ASAN/UBSAN.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/compressor_v2_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/compressor_v2_host_test.cpp" \
  "$SRC/Application/Audio/FxEngine/Compressor.cpp" \
  -lm \
  -o "$TMP/compressor_v2_host_test"
(cd "$ROOT" && "$TMP/compressor_v2_host_test")