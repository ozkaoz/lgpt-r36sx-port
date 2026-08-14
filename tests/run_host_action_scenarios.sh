#!/usr/bin/env bash
# run_host_action_scenarios.sh -- F8: runner de escenarios funcionales por
# vista contra el catalogo dorado de input (ScenarioCatalog + ActionMap +
# ChordResolver). Sin dependencias de la app. ASAN/UBSAN activados.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/action_scenarios_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$SRC/Application/UI/Input/ChordResolver.cpp" \
  "$SRC/Application/UI/Input/ActionMap.cpp" \
  "$ROOT/tests/host/scenario_runner_host_test.cpp" \
  -o "$TMP/scenario_runner_host_test"
(cd "$ROOT" && "$TMP/scenario_runner_host_test")
