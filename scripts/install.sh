#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/R36S/PORT LPTRACKER}"
SD="${SD_MOUNT:-/mnt/f}"
BUILD="$PROJECT_ROOT/BUILD/U2523"
CORE="$BUILD/lgpt_r36sx_u2523.so"
DAEMON="$BUILD/r36s_u2523_usb_audio_io"
ACTIVE_CORE="$SD/cubegm/cores/lgpt_r36sx_port_libretro.so"
ACTIVE_DAEMON="$SD/lgpt/otg/bin/r36s_u241_usb_audio_io"
MODULE="$SD/lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2/usb_f_uac2.ko"
TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="$PROJECT_ROOT/BACKUPS/LGPT_BEFORE_U2523_$TS"
fail(){ echo "ERROR: $*" >&2; exit 1; }
for f in "$CORE" "$DAEMON" "$ACTIVE_CORE" "$ACTIVE_DAEMON" "$MODULE"; do [[ -s "$f" ]] || fail "Missing $f"; done
mkdir -p "$BACKUP/otg_bin" "$SD/lgpt/tmp/record" "$SD/lgpt/samples/records" "$SD/LGPT_OTG_LOGS"
cp -f "$ACTIVE_CORE" "$BACKUP/lgpt_r36sx_port_libretro.previous.so"
cp -f "$ACTIVE_DAEMON" "$BACKUP/r36s_usb_audio_io.previous"
[[ -f "$SD/cubegm/lgpt" ]] && cp -f "$SD/cubegm/lgpt" "$BACKUP/lgpt_launcher.previous"
for f in otg_u241_common.sh otg_u241_setup_once.sh otg_u241_apply_profile_once.sh otg_u241_shutdown.sh; do
  [[ -f "$SD/lgpt/otg/bin/$f" ]] && cp -f "$SD/lgpt/otg/bin/$f" "$BACKUP/otg_bin/$f"
done
[[ -f "$SD/lgpt/otg/audio_usb_profile" ]] && cp -f "$SD/lgpt/otg/audio_usb_profile" "$BACKUP/audio_usb_profile.previous"
install -m 0755 "$CORE" "$ACTIVE_CORE"
install -m 0755 "$DAEMON" "$ACTIVE_DAEMON"
install -m 0755 "$ROOT/device/lgpt_launcher_u241.sh" "$SD/cubegm/lgpt"
for f in otg_u241_common.sh otg_u241_setup_once.sh otg_u241_apply_profile_once.sh otg_u241_shutdown.sh; do
  install -m 0755 "$ROOT/device/$f" "$SD/lgpt/otg/bin/$f"
done
printf 'MONO_48K\n' > "$SD/lgpt/otg/audio_usb_profile"
rm -f "$SD/lgpt/otg/lowlat_240" "$SD/lgpt/otg/disable_mute_local" "$SD/lgpt/otg/mute_local_during_otg"
cat > "$SD/LGPT_OTG_LOGS/INSTALL_STATE_U2523.txt" <<EOF
Installed: $(date -Is)
Version: U2.52.3
Core SHA256: $(sha256sum "$ACTIVE_CORE" | awk '{print $1}')
Daemon SHA256: $(sha256sum "$ACTIVE_DAEMON" | awk '{print $1}')
Backup: $BACKUP
EOF
sync
echo INSTALL_U2523_OK
echo "BACKUP=$BACKUP"
