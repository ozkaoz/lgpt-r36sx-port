#!/usr/bin/env bash
# MIXER_VU_CHAIN (U2.52.7): host test of the real mixer VU chain with a
# real BassSynth (PlayerChannel scan -> MixerMeters -> bar level), at the
# host rate (44.1 kHz) and the device rate (48 kHz).  Under ASAN/UBSAN.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/mixer_vu_chain.XXXXXX)"
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
  "$ROOT/tests/host/mixer_vu_chain_host_test.cpp" \
  "$SRC/Application/Player/PlayerChannel.cpp" \
  "$SRC/Application/Instruments/BassSynth.cpp" \
  "$SRC/Application/Instruments/FilterV2.cpp" \
  "$SRC/Application/Audio/InstrumentEq.cpp" \
  "$SRC/Application/Player/TablePlayback.cpp" \
  "$SRC/Foundation/Variables/Variable.cpp" \
  "$SRC/Foundation/Variables/VariableContainer.cpp" \
  "$SRC/Foundation/Observable.cpp" \
  -lm \
  -o "$TMP/mixer_vu_chain_host_test"
(cd "$ROOT" && "$TMP/mixer_vu_chain_host_test")