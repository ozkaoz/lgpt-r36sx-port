#!/usr/bin/env bash
# MASTER_SAFETY (U2.56, feedback #9): host test of the REAL AudioMixer
# master path (sum -> pre-clip meter -> safety limiter) with a real
# BassSynth, under ASAN/UBSAN.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/master_safety.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -DPLATFORM_TREEFROG -DTREEFROG_INPUT_PROFILE=0 -DTREEFROG_AUDIO_MODE=0 \
  -DTREEFROG_TIMER_MODE=1 -DTREEFROG_VIDEO_MODE=0 \
  -DCPP_MEMORY -DHAVE_STDINT_H -D_NDEBUG -D_NO_JACK_ -DDUMMYMIDI \
  -D_LGPT_NO_SCREEN_CACHE_ -include stdint.h \
  -include "$SRC/Adapters/TREEFROG/Compat/SDL_types_force.h" \
  -I"$SRC/Adapters/TREEFROG/Compat" \
  -I"$SRC" \
  "$ROOT/tests/host/master_safety_host_test.cpp" \
  "$SRC/Services/Audio/AudioMixer.cpp" \
  "$SRC/Application/Player/PlayerChannel.cpp" \
  "$SRC/Application/Instruments/BassSynth.cpp" \
  "$SRC/Application/Instruments/FilterV2.cpp" \
  "$SRC/Application/Audio/InstrumentEq.cpp" \
  "$SRC/Application/Player/TablePlayback.cpp" \
  "$SRC/Foundation/Variables/Variable.cpp" \
  "$SRC/Foundation/Variables/VariableContainer.cpp" \
  "$SRC/Foundation/Observable.cpp" \
  -lm \
  -o "$TMP/master_safety_host_test"
(cd "$ROOT" && "$TMP/master_safety_host_test")