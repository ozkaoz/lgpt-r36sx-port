#!/usr/bin/env bash
# F3-3b: ChopperView dibujo/grilla golden - equivalencia golden (header-only).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/chopper_draw_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/chopper_draw_host_test.cpp" \
  -o "$TMP/chopper_draw_host_test"
"$TMP/chopper_draw_host_test"
