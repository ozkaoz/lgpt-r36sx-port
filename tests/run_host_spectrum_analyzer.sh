#!/usr/bin/env bash
# EQ8_SPECTRUM_VERIFY (U2.57, feedback #10): compila el SpectrumAnalyzer.cpp
# REAL y verifica que los bins del analizador representan fielmente la señal
# del master mix (0 dBFS -> 0.25, -12 dBFS -> 4x menos, colocacion por barra).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/spectrum_analyzer_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/spectrum_analyzer_host_test.cpp" \
  "$SRC/Application/Audio/SpectrumAnalyzer.cpp" \
  -lm \
  -o "$TMP/spectrum_analyzer_host_test"
(cd "$ROOT" && "$TMP/spectrum_analyzer_host_test")