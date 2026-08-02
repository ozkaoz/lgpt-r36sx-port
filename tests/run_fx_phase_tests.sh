#!/usr/bin/env bash
cd "$(dirname "$0")/.."
for t in tests/test_fx_phase*.py; do
  echo "== $t =="
  python3 "$t" 2>&1 | tail -1
done
