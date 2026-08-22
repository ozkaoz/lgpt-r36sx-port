#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d /tmp/lgpt_startup_actions.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
echo "=== Static audit ==="
python3 "$ROOT/tests/test_startup_project_actions_static.py"
echo "=== Functional python ==="
python3 "$ROOT/tests/test_startup_duplicate_functional.py"
echo "=== C++ host test ==="
g++ -std=c++17 -Wall -Wextra -I"$ROOT/source/sources" "$ROOT/tests/host/startup_project_actions_host_test.cpp" -o "$TMP/test" -lstdc++fs 2>&1 || g++ -std=c++17 -Wall -Wextra -I"$ROOT/source/sources" "$ROOT/tests/host/startup_project_actions_host_test.cpp" -o "$TMP/test"
"$TMP/test"
echo "RUN_HOST_STARTUP_PROJECT_ACTIONS_OK"
