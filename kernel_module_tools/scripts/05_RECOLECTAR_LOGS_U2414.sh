#!/usr/bin/env bash
set -Eeuo pipefail

SD="${SD_MOUNT:-/mnt/f}"
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/R36S/PORT LPTRACKER}"
TS="$(date +%Y%m%d_%H%M%S)"
WORK="$PROJECT_ROOT/LOGS/LGPT_U2414_AU8_SYNC_$TS"
ZIP="$PROJECT_ROOT/LOGS/LGPT_U2414_AU8_SYNC_$TS.zip"
MODULE="$SD/lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2/usb_f_uac2.ko"

mkdir -p "$WORK"

{
    date -Is
    uname -a
    findmnt -T "$SD" || true
    df -hT "$SD" || true
    echo "=== AU8-SYNC MODULE ==="
    file "$MODULE" 2>&1 || true
    sha256sum "$MODULE" 2>&1 || true
    modinfo "$MODULE" 2>&1 || true
    strings "$MODULE" 2>/dev/null |
        grep -E 'R36SX_U2414|R36SX USB AUDIO' || true
    echo "=== SD TREE ==="
    find "$SD/LGPT_OTG_LOGS" "$SD/lgpt/otg" -maxdepth 8 \
        -printf '%M %s %TY-%Tm-%Td %TH:%TM:%TS %p\n' 2>&1 |
        sort || true
} > "$WORK/WSL_SD_STATE.txt"

if [[ -d "$SD/LGPT_OTG_LOGS" ]]; then
    cp -a "$SD/LGPT_OTG_LOGS" "$WORK/LGPT_OTG_LOGS" 2>/dev/null ||
        sudo cp -a "$SD/LGPT_OTG_LOGS" "$WORK/LGPT_OTG_LOGS"
fi

if [[ -d "$SD/lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2" ]]; then
    cp -a "$SD/lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2" \
        "$WORK/u2_38au8_sync_uac2" 2>/dev/null ||
        sudo cp -a \
            "$SD/lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2" \
            "$WORK/u2_38au8_sync_uac2"
fi

for f in "$SD/log.txt" "$SD/log.txt.prev" "$SD/lgpt/config.xml"; do
    [[ -e "$f" ]] || continue
    cp -a "$f" "$WORK/$(basename "$f")" 2>/dev/null ||
        sudo cp -a "$f" "$WORK/$(basename "$f")"
done

(cd "$(dirname "$WORK")" && zip -qr "$ZIP" "$(basename "$WORK")")
echo "LOG_PACKAGE=$ZIP"
