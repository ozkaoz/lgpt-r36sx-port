#!/usr/bin/env bash
# FXP_UNIFIED_FX (bacon-1.5, item 5): host test of the unified FX API
# (FxEngine::SetParam/GetParam + fxParamFromByte) under ASAN/UBSAN.
# Compiles the real FxEngine.cpp and its four DSP modules.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/unified_fx_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/unified_fx_host_test.cpp" \
  "$SRC/Application/Audio/FxEngine/FxEngine.cpp" \
  "$SRC/Application/Audio/FxEngine/DelayLine.cpp" \
  "$SRC/Application/Audio/FxEngine/Reverb.cpp" \
  "$SRC/Application/Audio/FxEngine/ParametricEQ.cpp" \
  "$SRC/Application/Audio/FxEngine/Compressor.cpp" \
  -lm \
  -o "$TMP/unified_fx_host_test"
(cd "$ROOT" && "$TMP/unified_fx_host_test")