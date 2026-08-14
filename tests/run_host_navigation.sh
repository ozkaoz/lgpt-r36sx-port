#!/usr/bin/env bash
# run_host_navigation.sh -- F2: compila y ejecuta el test host del
# NavigationController (stack de modales). Sin dependencias de la app.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources/Application/UI/Navigation"
TMP="${TMPDIR:-/tmp}/navigation_host"
rm -rf "$TMP" && mkdir -p "$TMP"
g++ -std=gnu++03 -Wall -Wextra -Werror \
  -I"$ROOT/source/sources" \
  "$SRC/NavigationController.cpp" \
  "$ROOT/tests/host/navigation_host_test.cpp" \
  -o "$TMP/navigation_host_test"
"$TMP/navigation_host_test"