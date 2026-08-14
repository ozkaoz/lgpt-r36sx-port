#!/usr/bin/env bash
# F3-4c: MixerMenu menu L1+A del Mixer - oraculos golden (header-only).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/mixer_menu_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/mixer_menu_host_test.cpp" \
  -o "$TMP/mixer_menu_host_test"
(cd "$ROOT" && "$TMP/mixer_menu_host_test")
