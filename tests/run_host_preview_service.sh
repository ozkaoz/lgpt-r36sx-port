#!/usr/bin/env bash
# F3-3a: PreviewService - equivalencia golden bajo ASAN/UBSAN (header-only).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/preview_service_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/preview_service_host_test.cpp" \
  -o "$TMP/preview_service_host_test"
"$TMP/preview_service_host_test"