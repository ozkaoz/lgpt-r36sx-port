#!/usr/bin/env bash
# SAMPLE_EQ_EDIT (U2.52.8, feedback "C"): a REAL SampleInstrument over a
# TestPool-injected SoundSource; SIP_EQ* edits between renders must never
# kill the sound (regression of the RBJ bell DC-shelf blowup, now fixed by
# BACON_1.5_BELL_PREWARPED).  Under ASAN/UBSAN.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/sample_eq_edit.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra -fpermissive \
  -DPLATFORM_TREEFROG -DTREEFROG_INPUT_PROFILE=0 -DTREEFROG_AUDIO_MODE=0 \
  -DTREEFROG_TIMER_MODE=1 -DTREEFROG_VIDEO_MODE=0 \
  -DCPP_MEMORY -DHAVE_STDINT_H -D_NDEBUG -D_NO_JACK_ -DDUMMYMIDI \
  -D_LGPT_NO_SCREEN_CACHE_ -include stdint.h \
  -include "$SRC/Adapters/TREEFROG/Compat/SDL_types_force.h" \
  -I"$SRC/Adapters/TREEFROG/Compat" \
  -I"$SRC" \
  "$ROOT/tests/host/sample_eq_edit_host_test.cpp" \
  "$SRC/Application/Instruments/SampleInstrument.cpp" \
  "$SRC/Application/Instruments/BassSynth.cpp" \
  "$SRC/Application/Instruments/SampleVariable.cpp" \
  "$SRC/Application/Instruments/SRPUpdaters.cpp" \
  "$SRC/Application/Instruments/FilterV2.cpp" \
  "$SRC/Application/Audio/InstrumentEq.cpp" \
  "$SRC/Application/Player/TablePlayback.cpp" \
  "$SRC/Foundation/Variables/Variable.cpp" \
  "$SRC/Foundation/Variables/WatchedVariable.cpp" \
  "$SRC/Foundation/Variables/VariableContainer.cpp" \
  "$SRC/Foundation/Observable.cpp" \
  -lm \
  -o "$TMP/sample_eq_edit_host_test"
(cd "$ROOT" && "$TMP/sample_eq_edit_host_test")