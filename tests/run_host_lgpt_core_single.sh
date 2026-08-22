#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d /tmp/lgpt_core_single.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
g++ -std=c++17 -Wall -Wextra "$ROOT/tests/host/lgpt_core_single_entry_host_test.cpp" -o "$TMP/test"
"$TMP/test"
g++ -std=c++17 -Wall -Wextra "$ROOT/tests/host/canonical_lgpt_core_migration_host_test.cpp" -o "$TMP/test2"
"$TMP/test2"
g++ -std=c++17 -Wall -Wextra "$ROOT/tests/host/canonical_core_binary_identity_host_test.cpp" -o "$TMP/test3"
"$TMP/test3"
echo "RUN_HOST_LGPT_CORE_SINGLE_OK"