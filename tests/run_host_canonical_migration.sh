#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d /tmp/canonical_mig.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
g++ -std=c++17 -Wall -Wextra "$ROOT/tests/host/canonical_lgpt_core_migration_host_test.cpp" -o "$TMP/test_mig"
"$TMP/test_mig"
g++ -std=c++17 -Wall -Wextra "$ROOT/tests/host/canonical_core_binary_identity_host_test.cpp" -o "$TMP/test_bin"
"$TMP/test_bin"
echo "RUN_HOST_CANONICAL_MIGRATION_OK"