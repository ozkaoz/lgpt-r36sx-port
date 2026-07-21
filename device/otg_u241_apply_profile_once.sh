#!/bin/sh
BASE=/mnt/sdcard/lgpt/otg
LOGROOT=/mnt/sdcard/LGPT_OTG_LOGS
mkdir -p "$LOGROOT" 2>/dev/null || true

MODE="${1:-LOCAL_CONSOLE}"
echo "$MODE" > "$BASE/audio_driver_mode" 2>/dev/null || true

{
    echo "MODE_APPLY=$MODE DATE=$(date)"
    "$BASE/bin/otg_u241_setup_once.sh"
} >> "$LOGROOT/U2517_APPLY_MODE.log" 2>&1 &

exit 0
