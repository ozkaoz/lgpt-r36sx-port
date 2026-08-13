#!/usr/bin/env bash
# F3-3c: ChopperController flujos de edicion golden - equivalencia golden (header-only).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/chopper_controller_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/chopper_controller_host_test.cpp" \
  -o "$TMP/chopper_controller_host_test"
"$TMP/chopper_controller_host_test"