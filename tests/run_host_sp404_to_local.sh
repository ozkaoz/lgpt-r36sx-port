#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/sp404_to_local.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
g++ -std=c++17 -g -Wall -Wextra -I"$SRC" "$ROOT/tests/host/sp404_to_local_requires_full_apply_host_test.cpp" -o "$TMP/test"
"$TMP/test"
g++ -std=c++17 -g -Wall -Wextra -I"$SRC" "$ROOT/tests/host/local_never_fast_host_test.cpp" -o "$TMP/test2"
"$TMP/test2"
g++ -std=c++17 -g -Wall -Wextra -I"$SRC" "$ROOT/tests/host/sp404_fifo_keepalive_exit_host_test.cpp" -o "$TMP/test3"
"$TMP/test3"
g++ -std=c++17 -g -Wall -Wextra -I"$SRC" "$ROOT/tests/host/host_runtime_cleanup_host_test.cpp" -o "$TMP/test4"
"$TMP/test4"
