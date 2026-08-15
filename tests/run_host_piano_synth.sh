#!/usr/bin/env bash
# PIANO_SYNTH (bacon-1.5, item 7): host DSP test of the polyphonic additive
# piano instrument.  Compiles the real PianoSynth.cpp with FilterV2 +
# InstrumentEq and the Foundation pieces under ASAN/UBSAN.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/piano_synth_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/piano_synth_host_test.cpp" \
  "$SRC/Application/Instruments/PianoSynth.cpp" \
  "$SRC/Application/Instruments/FilterV2.cpp" \
  "$SRC/Application/Audio/InstrumentEq.cpp" \
  "$SRC/Application/Player/TablePlayback.cpp" \
  "$SRC/Foundation/Variables/Variable.cpp" \
  "$SRC/Foundation/Variables/VariableContainer.cpp" \
  "$SRC/Foundation/Observable.cpp" \
  -lm \
  -o "$TMP/piano_synth_host_test"
(cd "$ROOT" && "$TMP/piano_synth_host_test")