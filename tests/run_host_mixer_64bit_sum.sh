#!/usr/bin/env bash
# F7-64BIT: AudioMixer master sum en 64 bits (BACON_1.5_64BIT_MASTER_SUM) -
# compila el AudioMixer.cpp REAL (los otros tests host lo stubean).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/mixer_64bit_sum_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/mixer_64bit_sum_host_test.cpp" \
  "$SRC/Services/Audio/AudioMixer.cpp" \
  -lm \
  -o "$TMP/mixer_64bit_sum_host_test"
(cd "$ROOT" && "$TMP/mixer_64bit_sum_host_test")
