#!/usr/bin/env bash
# F4d: AudioBackend - registro declarativo de clases de backend + contrato
# de operaciones (objetivo 6).  Oraculos golden de los daemons reales.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/audio_backend_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/audio_backend_host_test.cpp" \
  -o "$TMP/audio_backend_host_test"
(cd "$ROOT" && "$TMP/audio_backend_host_test")
