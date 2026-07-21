#!/usr/bin/env bash
set -Eeuo pipefail
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/R36S/PORT LPTRACKER}"
SD="${SD_MOUNT:-/mnt/f}"
BUILD="$PROJECT_ROOT/BUILD/U2523"
CORE_BUILD="$BUILD/lgpt_r36sx_u2523.so"
DAEMON_BUILD="$BUILD/r36s_u2523_usb_audio_io"
CORE="$SD/cubegm/cores/lgpt_r36sx_port_libretro.so"
DAEMON="$SD/lgpt/otg/bin/r36s_u241_usb_audio_io"
MODULE="$SD/lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2/usb_f_uac2.ko"
ERRORS=0
for f in "$CORE_BUILD" "$DAEMON_BUILD" "$CORE" "$DAEMON" "$MODULE" "$SD/cubegm/lgpt"; do [[ -s "$f" ]] || ERRORS=$((ERRORS+1)); done
for marker in U2523_RENAME_CARET_ALIGNMENT_GITHUB_FINAL U2520_PENDING_TAKE_RECORD_EDITOR_PHYSICAL_EDGE_READY R36SX_CAPTURE_ABI=2; do grep -aFq "$marker" "$CORE" || ERRORS=$((ERRORS+1)); done
grep -aFq 'R36SX_USB_AUDIO_DAEMON_ABI=7' "$DAEMON" || ERRORS=$((ERRORS+1))
grep -aFq R36SX_U2414_AU8_SYNC_REPLICA "$MODULE" || ERRORS=$((ERRORS+1))
[[ "$(sha256sum "$CORE_BUILD"|awk '{print $1}')" == "$(sha256sum "$CORE"|awk '{print $1}')" ]] || ERRORS=$((ERRORS+1))
[[ "$(sha256sum "$DAEMON_BUILD"|awk '{print $1}')" == "$(sha256sum "$DAEMON"|awk '{print $1}')" ]] || ERRORS=$((ERRORS+1))
[[ "$(cat "$SD/lgpt/otg/audio_usb_profile" 2>/dev/null)" == MONO_48K ]] || ERRORS=$((ERRORS+1))
echo "ERRORS=$ERRORS"
[[ "$ERRORS" -eq 0 ]]
echo VERIFY_U2523_OK
