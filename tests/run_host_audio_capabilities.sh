#!/usr/bin/env bash
# F4b: vocabulario de capacidades de audio + derivacion per-modo.
# Oraculos derivados de los primitivos golden del bridge (golden Bacon 1.2.1).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/audio_capabilities_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/audio_capabilities_host_test.cpp" \
  -o "$TMP/audio_capabilities_host_test"
(cd "$ROOT" && "$TMP/audio_capabilities_host_test")
