#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -f "$SCRIPT_DIR/../../cubegm/lgpt" ]]; then
    ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
    OUT="${1:-$ROOT/dist/LGPT_R36SX_U2524_COPYROOT_UAC2_FULL_SOURCE.zip}"
    "$SCRIPT_DIR/refresh_manifest.sh" "$ROOT"
    "$SCRIPT_DIR/verify_copy_root_layout.sh" "$ROOT"
    "$ROOT/SOURCE_AND_TOOLS/tests/run_all.sh"
    python3 "$SCRIPT_DIR/build_copy_root_release.py" "$ROOT" "$OUT"
else
    exec "$SCRIPT_DIR/build_from_full_clone.sh" "$@"
fi
