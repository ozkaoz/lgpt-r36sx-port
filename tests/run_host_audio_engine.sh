#!/usr/bin/env bash
# F4e: AudioEngine - politica de estado del motor de audio (capa pura).
# Oraculos golden de TreeFrogUac2Bridge.cpp (golden Bacon 1.2.1).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/audio_engine_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/audio_engine_host_test.cpp" \
  -o "$TMP/audio_engine_host_test"
(cd "$ROOT" && "$TMP/audio_engine_host_test")
