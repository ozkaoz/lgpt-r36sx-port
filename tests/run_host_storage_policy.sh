#!/usr/bin/env bash
# F5: StoragePolicy - politica de storage/SD estricta (capa pura).
# Oraculos golden: clasificacion Volatile/Persistent/Diagnostic de cada
# ruta real de source/ y device/ + regla "nada nuevo escribe fuera".
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/storage_policy_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/storage_policy_host_test.cpp" \
  -o "$TMP/storage_policy_host_test"
(cd "$ROOT" && "$TMP/storage_policy_host_test")