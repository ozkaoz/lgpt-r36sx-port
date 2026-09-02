#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/Toolchains/R36SX}"
WORK="${WORK_ROOT:-$PROJECT_ROOT/WORK/U2523_SOURCE}"
rm -rf "$WORK"
mkdir -p "$WORK"
rsync -a "$ROOT/source/" "$WORK/"
echo PREPARE_SOURCE_OK
echo "SOURCE=$WORK"
