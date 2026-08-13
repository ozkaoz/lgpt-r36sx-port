#!/usr/bin/env bash
# F3-2: PitchEnvelopeTool - equivalencia golden bajo ASAN/UBSAN (header-only).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/pitch_tool_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/pitch_tool_host_test.cpp" \
  -o "$TMP/pitch_tool_host_test"
"$TMP/pitch_tool_host_test"