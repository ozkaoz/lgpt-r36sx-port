#!/usr/bin/env bash
set -Eeuo pipefail

SD="${SD_MOUNT:-/mnt/f}"
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/Toolchains/R36SX}"
BIN="$SD/lgpt/otg/bin"
DIR="${1:-}"

run_sd(){ "$@" 2>/dev/null || sudo "$@"; }
fail(){ echo "ERROR: $*" >&2; exit 1; }

if [[ -z "$DIR" ]]; then
    DIR="$(
        find "$PROJECT_ROOT/BACKUPS" -maxdepth 1 -type d \
          -name 'LGPT_U2413_BEFORE_U2414_*' \
          -printf '%T@ %p\n' 2>/dev/null |
        sort -nr | head -n1 | cut -d' ' -f2- || true
    )"
fi

[[ -d "$DIR" ]] || fail "No se encontró backup U2.41.3."

for f in otg_u241_common.sh otg_u241_setup_once.sh \
         otg_u241_apply_profile_once.sh otg_u241_shutdown.sh; do
    [[ -s "$DIR/$f" ]] || fail "Falta $DIR/$f"
    run_sd cp -f "$DIR/$f" "$BIN/$f"
    run_sd chmod 0755 "$BIN/$f"
done

printf 'LOCAL_CONSOLE\n' > /tmp/u2414_restore
run_sd cp -f /tmp/u2414_restore "$SD/lgpt/otg/audio_driver_mode"
sync

echo "RESTORE_U2413_RUNTIME_OK"
echo "SOURCE=$DIR"
