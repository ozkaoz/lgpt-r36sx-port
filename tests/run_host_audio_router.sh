#!/usr/bin/env bash
# F4c: AudioRouter - politica declarativa de seleccion/routing de backends.
# Oraculos golden de TreeFrogUac2Bridge.cpp (golden Bacon 1.2.1).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/audio_router_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/audio_router_host_test.cpp" \
  -o "$TMP/audio_router_host_test"
(cd "$ROOT" && "$TMP/audio_router_host_test")
