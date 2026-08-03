#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="${TMPDIR:-/tmp}/lgpt_fx_repro"
mkdir -p "$TMP"
BASE="-std=gnu++03 -DPLATFORM_TREEFROG -DCPP_MEMORY -DHAVE_STDINT_H -D_NDEBUG -D_NO_JACK_ -DDUMMYMIDI -include stdint.h"
g++ -O2 $BASE -I"$ROOT/source/sources" \
    "$ROOT/tests/fx_repro_engage.cpp" \
    "$ROOT/source/sources/Application/Audio/FxEngine/DelayLine.cpp" \
    "$ROOT/source/sources/Application/Audio/FxEngine/Reverb.cpp" \
    "$ROOT/source/sources/Application/Audio/FxEngine/ParametricEQ.cpp" \
    "$ROOT/source/sources/Application/Audio/FxEngine/Compressor.cpp" \
    "$ROOT/source/sources/Application/Audio/FxEngine/FxEngine.cpp" \
    -o "$TMP/fx_repro"
"$TMP/fx_repro"
