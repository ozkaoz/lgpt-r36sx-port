#!/usr/bin/env bash
# ANALYZER_TARGET (bacon-1.5, item 7): host test of the SpectrumAnalyzer
# targeted-tap contract (feed only when armed AND instrument == target).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/analyzer_target_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/analyzer_target_host_test.cpp" \
  "$SRC/Application/Audio/SpectrumAnalyzer.cpp" \
  "$SRC/Foundation/Observable.cpp" \
  "$SRC/Foundation/Variables/Variable.cpp" \
  "$SRC/Foundation/Variables/VariableContainer.cpp" \
  -lm \
  -o "$TMP/analyzer_target_host_test"
"$TMP/analyzer_target_host_test"