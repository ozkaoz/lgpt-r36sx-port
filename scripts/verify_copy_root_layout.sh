#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ $# -ge 1 ]]; then
    ROOT="$(cd "$1" && pwd)"
elif [[ -f "$SCRIPT_DIR/../../cubegm/lgpt" ]]; then
    ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
elif [[ -f "$SCRIPT_DIR/../sd_root/cubegm/lgpt" ]]; then
    ROOT="$(cd "$SCRIPT_DIR/../sd_root" && pwd)"
else
    echo 'ERROR: no se encontró el payload copy-root.' >&2
    exit 2
fi

errors=0
need_file() {
    rel="$1"
    if [[ -s "$ROOT/$rel" ]]; then
        echo "FILE_OK $rel"
    else
        echo "FILE_MISSING_OR_EMPTY $rel" >&2
        errors=$((errors + 1))
    fi
}
need_dir() {
    rel="$1"
    if [[ -d "$ROOT/$rel" ]]; then
        echo "DIR_OK $rel"
    else
        echo "DIR_MISSING $rel" >&2
        errors=$((errors + 1))
    fi
}

need_file cubegm/lgpt
need_file lgpt/config.xml
need_file lgpt/config.stock.xml
need_file roms/lgpt/start.lgpt
need_dir lgpt/samples
need_dir lgpt/instruments
need_dir lgpt/projects

grep -F 'copy-root + ALSA/UAC2 validated' "$ROOT/cubegm/lgpt" >/dev/null || {
    echo 'LAUNCHER_MARKER_MISSING' >&2
    errors=$((errors + 1))
}
grep -F '<SAMPLELIB value="/mnt/sdcard/lgpt/samples" />' "$ROOT/lgpt/config.xml" >/dev/null || {
    echo 'CONFIG_SAMPLELIB_BAD' >&2
    errors=$((errors + 1))
}

(( errors == 0 )) || {
    echo "VERIFY_LAYOUT_FAILED errors=$errors" >&2
    exit 5
}
echo "VERIFY_LAYOUT_OK root=$ROOT"
