#!/usr/bin/env bash
set -Eeuo pipefail
# NOTE (2026-09-01): PROJECT_ROOT points to the BACKUPS area which STILL lives at
# /mnt/d/R36S/PORT LPTRACKER/BACKUPS (SD snapshots + git bundles were kept there).
# Build sources moved to /mnt/d/Toolchains/R36SX — see docs/TOOLCHAINS.md
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/R36S/PORT LPTRACKER}"
SD="${SD_MOUNT:-/mnt/f}"
BACKUP="${1:-}"
if [[ -z "$BACKUP" ]]; then
  BACKUP="$(find "$PROJECT_ROOT/BACKUPS" -maxdepth 1 -type d -name 'LGPT_BEFORE_U2523_*' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n1 | cut -d' ' -f2-)"
fi
[[ -s "$BACKUP/lgpt_r36sx_port_libretro.previous.so" ]]
[[ -s "$BACKUP/r36s_usb_audio_io.previous" ]]
cp -f "$BACKUP/lgpt_r36sx_port_libretro.previous.so" "$SD/cubegm/cores/lgpt_r36sx_port_libretro.so"
cp -f "$BACKUP/r36s_usb_audio_io.previous" "$SD/lgpt/otg/bin/r36s_u241_usb_audio_io"
[[ -f "$BACKUP/lgpt_launcher.previous" ]] && cp -f "$BACKUP/lgpt_launcher.previous" "$SD/cubegm/lgpt"
for f in "$BACKUP/otg_bin/"*.sh; do [[ -f "$f" ]] && cp -f "$f" "$SD/lgpt/otg/bin/$(basename "$f")"; done
[[ -f "$BACKUP/audio_usb_profile.previous" ]] && cp -f "$BACKUP/audio_usb_profile.previous" "$SD/lgpt/otg/audio_usb_profile"
sync
echo RESTORE_PREVIOUS_OK
