#!/usr/bin/env bash
# FXP_FILTER_V2 (bacon-1.5, item 2): host test of the TPT state-variable
# filter (spectral + stability).  Compiles FilterV2.cpp together with the
# test under ASAN/UBSAN.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/filterv2_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/filterv2_host_test.cpp" \
  "$SRC/Application/Instruments/FilterV2.cpp" \
  -lm \
  -o "$TMP/filterv2_host_test"
(cd "$ROOT" && "$TMP/filterv2_host_test")