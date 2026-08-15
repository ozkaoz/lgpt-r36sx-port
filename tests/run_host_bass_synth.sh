#!/usr/bin/env bash
# BASS_SYNTH (bacon-1.5, item 6): host DSP test of the native BassSynth.
# Compiles the real BassSynth.cpp with FilterV2 + InstrumentEq and the
# Foundation pieces (Variable/VariableContainer/Observable) under ASAN/UBSAN.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/bass_synth_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/bass_synth_host_test.cpp" \
  "$SRC/Application/Instruments/BassSynth.cpp" \
  "$SRC/Application/Instruments/FilterV2.cpp" \
  "$SRC/Application/Audio/InstrumentEq.cpp" \
  "$SRC/Application/Player/TablePlayback.cpp" \
  "$SRC/Foundation/Variables/Variable.cpp" \
  "$SRC/Foundation/Variables/VariableContainer.cpp" \
  "$SRC/Foundation/Observable.cpp" \
  -lm \
  -o "$TMP/bass_synth_host_test"
(cd "$ROOT" && "$TMP/bass_synth_host_test")