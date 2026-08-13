#!/usr/bin/env bash
# F3-4d: FxNavigator navegacion/edicion de paginas FX - oraculos golden.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/fx_navigator_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/fx_navigator_host_test.cpp" \
  -o "$TMP/fx_navigator_host_test"
(cd "$ROOT" && "$TMP/fx_navigator_host_test")
