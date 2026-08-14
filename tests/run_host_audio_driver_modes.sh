#!/usr/bin/env bash
# F4a: tabla declarativa de modos de audio del driver UAC2 (capa pura).
# Oraculos golden de TreeFrogUac2Bridge.cpp (golden Bacon 1.2.1).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/audio_driver_modes_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/audio_driver_modes_host_test.cpp" \
  -o "$TMP/audio_driver_modes_host_test"
(cd "$ROOT" && "$TMP/audio_driver_modes_host_test")
