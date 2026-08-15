#!/usr/bin/env bash
# FXP_DELAY_REVERB_V2 (bacon-1.5, item 3): host test of the DelayLine V2
# (musical sync, loop filters, per-sample glide) and Reverb V2 (fractional
# delays, control-rate RT60, LFO modulation) DSP under ASAN/UBSAN.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/delay_reverb_v2_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/delay_reverb_v2_host_test.cpp" \
  "$SRC/Application/Audio/FxEngine/DelayLine.cpp" \
  "$SRC/Application/Audio/FxEngine/Reverb.cpp" \
  -lm \
  -o "$TMP/delay_reverb_v2_host_test"
(cd "$ROOT" && "$TMP/delay_reverb_v2_host_test")
