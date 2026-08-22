#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/android_aoa_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -I"$SRC" "$ROOT/tests/host/android_aoa_host_test.cpp" -o "$TMP/android_aoa_host_test"
(cd "$ROOT" && "$TMP/android_aoa_host_test")
