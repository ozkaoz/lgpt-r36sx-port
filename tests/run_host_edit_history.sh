#!/usr/bin/env bash
# F3-2: SampleEditHistory - equivalencia golden bajo ASAN/UBSAN (header-only).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/edit_history_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/edit_history_host_test.cpp" \
  -o "$TMP/edit_history_host_test"
"$TMP/edit_history_host_test"