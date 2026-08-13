#!/usr/bin/env bash
# run_host_input_policy.sh -- F1: compila y ejecuta el test host del
# catalogo de input (ActionMap/ChordResolver). Sin dependencias de la app.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources/Application/UI/Input"
TMP="${TMPDIR:-/tmp}/input_policy_host"
rm -rf "$TMP" && mkdir -p "$TMP"
g++ -std=gnu++03 -Wall -Wextra -Werror \
  -I"$ROOT/source/sources" \
  "$SRC/ChordResolver.cpp" "$SRC/ActionMap.cpp" \
  "$ROOT/tests/host/input_policy_host_test.cpp" \
  -o "$TMP/input_policy_host_test"
"$TMP/input_policy_host_test"

# F1b-Mixer: enumeracion exhaustiva CTX_MIXER/CTX_MIXER_FX (8192 masks)
# contra el modelo golden de MixerView.cpp:534-697.
g++ -std=gnu++03 -Wall -Wextra -Werror \
  -I"$ROOT/source/sources" \
  "$SRC/ChordResolver.cpp" "$SRC/ActionMap.cpp" \
  "$ROOT/tests/host/mixer_golden_enum.cpp" \
  -o "$TMP/mixer_golden_enum"
"$TMP/mixer_golden_enum"
