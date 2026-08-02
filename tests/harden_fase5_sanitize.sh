#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="${TMPDIR:-/tmp}/lgpt_fx_sanitize"
mkdir -p "$TMP"
BASE="-std=gnu++03 -DPLATFORM_TREEFROG -DCPP_MEMORY -DHAVE_STDINT_H -D_NDEBUG -D_NO_JACK_ -DDUMMYMIDI -include stdint.h"
g++ -O1 -g $BASE -I"$ROOT/source/sources" -fsanitize=address,undefined -Wall -Wextra -Werror \
    "$ROOT/tests/fx_sanitize_harness.cpp" \
    "$ROOT/source/sources/Application/Audio/FxEngine/DelayLine.cpp" \
    "$ROOT/source/sources/Application/Audio/FxEngine/Reverb.cpp" \
    "$ROOT/source/sources/Application/Audio/FxEngine/ParametricEQ.cpp" \
    "$ROOT/source/sources/Application/Audio/FxEngine/Compressor.cpp" \
    -o "$TMP/fx_sanitize"
ASAN_OPTIONS=detect_leaks=1 "$TMP/fx_sanitize"
echo HARDEN_FASE5_SANITIZERS_OK
