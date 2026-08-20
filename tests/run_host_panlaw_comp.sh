#!/usr/bin/env bash
# PANLAW_COMP (U2.57, feedback #10): compila la tabla panlaw REAL y verifica
# la compensacion de la ley de pan del instrumento (centro = 0 dB, hard pan
# = +3 dB, simetria, sin wrap) y la cadena vol 128 * pan centro = 0 dBFS.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/panlaw_comp_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/panlaw_comp_host_test.cpp" \
  -lm \
  -o "$TMP/panlaw_comp_host_test"
(cd "$ROOT" && "$TMP/panlaw_comp_host_test")