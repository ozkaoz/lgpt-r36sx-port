#!/usr/bin/env bash
# F3-4a: FxPages capa pura de las paginas FX del Mixer - oraculos golden (header-only).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/fx_pages_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/fx_pages_host_test.cpp" \
  -o "$TMP/fx_pages_host_test"
(cd "$ROOT" && "$TMP/fx_pages_host_test")
