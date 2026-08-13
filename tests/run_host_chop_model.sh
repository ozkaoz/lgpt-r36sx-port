#!/usr/bin/env bash
# F3-1: ChopModel - equivalencia golden bajo ASAN/UBSAN (header-only).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/chop_model_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/chop_model_host_test.cpp" \
  -o "$TMP/chop_model_host_test"
"$TMP/chop_model_host_test"