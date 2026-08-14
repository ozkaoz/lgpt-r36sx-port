#!/usr/bin/env bash
# F3-5a: PhraseGridEdit - logica pura de grid/edicion de la Phrase.
# Oraculos golden de PhraseView.cpp (golden Bacon 1.2.1).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/phrase_grid_edit_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/phrase_grid_edit_host_test.cpp" \
  -o "$TMP/phrase_grid_edit_host_test"
(cd "$ROOT" && "$TMP/phrase_grid_edit_host_test")