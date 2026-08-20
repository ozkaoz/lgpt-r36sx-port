#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/R36S/PORT LPTRACKER}"
SD="${SD_MOUNT:-/mnt/f}"
BUILD="$PROJECT_ROOT/BUILD/U2523"
CORE="$BUILD/lgpt_r36sx_u2523.so"
DAEMON="$BUILD/r36s_u2523_usb_audio_io"
SP404_DAEMON="$BUILD/r36s_sp404_host_audio_io"
MIDI_DAEMON="$BUILD/r36s_midi_host_io"
HOST_AUDIO_MODULE="$PROJECT_ROOT/BUILD/HOST_USB_AUDIO/snd-usb-audio.ko"
HOST_USBMIDI_MODULE="$PROJECT_ROOT/BUILD/HOST_USB_AUDIO/snd-usbmidi-lib.ko"
# ALSA core stack modules required before snd-usbmidi-lib/snd-usb-audio load.
HOST_CORE_MODULE_SRC="$PROJECT_ROOT/BUILD/HOST_USB_AUDIO"
HOST_CORE_MODULES="snd.ko snd-timer.ko snd-pcm.ko snd-hwdep.ko snd-seq-device.ko snd-rawmidi.ko"
ACTIVE_CORE="$SD/cubegm/cores/lgpt_r36sx_port_libretro.so"
ACTIVE_DAEMON="$SD/lgpt/otg/bin/r36s_u241_usb_audio_io"
ACTIVE_SP404_DAEMON="$SD/lgpt/otg/bin/r36s_sp404_host_audio_io"
ACTIVE_MIDI_DAEMON="$SD/lgpt/otg/bin/r36s_midi_host_io"
MODULE="$SD/lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2/usb_f_uac2.ko"
HOST_MODULE_DIR="$SD/lgpt/otg/modules/4.4.186-release/host_usb_audio"
ACTIVE_HOST_AUDIO_MODULE="$HOST_MODULE_DIR/snd-usb-audio.ko"
ACTIVE_HOST_USBMIDI_MODULE="$HOST_MODULE_DIR/snd-usbmidi-lib.ko"
TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="$PROJECT_ROOT/BACKUPS/LGPT_BEFORE_U2523_$TS"
fail(){ echo "ERROR: $*" >&2; exit 1; }
for f in "$CORE" "$DAEMON" "$ACTIVE_CORE" "$ACTIVE_DAEMON" "$MODULE"; do [[ -s "$f" ]] || fail "Missing $f"; done
if [[ -s "$SP404_DAEMON" && -s "$MIDI_DAEMON" ]]; then
  HOST_BACKENDS=1
else
  echo "WARN: host-side backends not built; run scripts/build_host_backends.sh"
  HOST_BACKENDS=0
fi
if [[ -s "$HOST_AUDIO_MODULE" && -s "$HOST_USBMIDI_MODULE" ]]; then
  HOST_MODULES=1
else
  echo "WARN: host USB audio modules not built; run kernel_module_tools/scripts/02_COMPILAR_HOST_USB_AUDIO.sh"
  HOST_MODULES=0
fi
mkdir -p "$BACKUP/otg_bin" "$SD/lgpt/tmp/record" "$SD/lgpt/samples/records" "$SD/LGPT_OTG_LOGS"
cp -f "$ACTIVE_CORE" "$BACKUP/lgpt_r36sx_port_libretro.previous.so"
cp -f "$ACTIVE_DAEMON" "$BACKUP/r36s_usb_audio_io.previous"
[[ -f "$SD/cubegm/lgpt" ]] && cp -f "$SD/cubegm/lgpt" "$BACKUP/lgpt_launcher.previous"
for f in otg_u241_common.sh otg_u241_setup_once.sh otg_u241_apply_profile_once.sh otg_u241_shutdown.sh otg_h37_apply_driver_mode.sh otg_h37_android_runtime_supervisor.sh otg_h37_host_runtime_supervisor.sh otg_h37_host_device_detect.sh; do
  [[ -f "$SD/lgpt/otg/bin/$f" ]] && cp -f "$SD/lgpt/otg/bin/$f" "$BACKUP/otg_bin/$f"
done
[[ -f "$SD/lgpt/otg/audio_usb_profile" ]] && cp -f "$SD/lgpt/otg/audio_usb_profile" "$BACKUP/audio_usb_profile.previous"
install -m 0755 "$CORE" "$ACTIVE_CORE"
install -m 0755 "$DAEMON" "$ACTIVE_DAEMON"
install -m 0755 "$ROOT/device/lgpt_launcher_u241.sh" "$SD/cubegm/lgpt"
for f in otg_u241_common.sh otg_u241_setup_once.sh otg_u241_apply_profile_once.sh otg_u241_shutdown.sh otg_h37_apply_driver_mode.sh otg_h37_android_runtime_supervisor.sh otg_h37_host_runtime_supervisor.sh otg_h37_host_device_detect.sh; do
  install -m 0755 "$ROOT/device/$f" "$SD/lgpt/otg/bin/$f"
done
if [[ "$HOST_BACKENDS" -eq 1 ]]; then
  install -m 0755 "$SP404_DAEMON" "$ACTIVE_SP404_DAEMON"
  install -m 0755 "$MIDI_DAEMON" "$ACTIVE_MIDI_DAEMON"
fi
if [[ "$HOST_MODULES" -eq 1 ]]; then
  mkdir -p "$HOST_MODULE_DIR"
  install -m 0644 "$HOST_AUDIO_MODULE" "$ACTIVE_HOST_AUDIO_MODULE"
  install -m 0644 "$HOST_USBMIDI_MODULE" "$ACTIVE_HOST_USBMIDI_MODULE"
  for c in $HOST_CORE_MODULES; do
    if [[ -s "$HOST_CORE_MODULE_SRC/$c" ]]; then
      install -m 0644 "$HOST_CORE_MODULE_SRC/$c" "$HOST_MODULE_DIR/$c"
    fi
  done
fi
printf 'STEREO_48K\n' > "$SD/lgpt/otg/audio_usb_profile"
: > "$SD/lgpt/otg/enable_lgpt_uac2_bridge"
printf 'LOCAL_CONSOLE\n' > "$SD/lgpt/otg/audio_driver_mode"
rm -f "$SD/lgpt/otg/lowlat_240" "$SD/lgpt/otg/disable_mute_local" "$SD/lgpt/otg/mute_local_during_otg"
# BACON_1.5_CONFIG_THEME (U2.58, feedback #11 "sigue viendose rosado"): the
# SD carried lgpt/config.xml with the STOCK magenta palette (BORDER FF008C,
# HICOLOR2 DB33DB, ROWCOLOR2 FF00FF) which overrides the in-code theme via
# defineColor() -- the whole port rendered pink.  The release config.xml
# (sd_root/lgpt/config.xml) is the classic blue-violet theme of Bacon-1.4;
# config.stock.xml stays installed as the launcher's fallback template
# (lgpt_launcher_u241.sh fails with code 21 when BOTH are missing).
install -m 0644 "$ROOT/sd_root/lgpt/config.stock.xml" "$SD/lgpt/config.stock.xml"
if [[ ! -f "$SD/lgpt/config.xml" ]]; then
  install -m 0644 "$ROOT/sd_root/lgpt/config.xml" "$SD/lgpt/config.xml"
elif grep -q 'FF008C' "$SD/lgpt/config.xml"; then
  cp -f "$SD/lgpt/config.xml" "$BACKUP/config.xml.pink.previous"
  install -m 0644 "$ROOT/sd_root/lgpt/config.xml" "$SD/lgpt/config.xml"
  echo "CONFIG_PINK_REPAIRED"
fi
cat > "$SD/LGPT_OTG_LOGS/INSTALL_STATE_U2523.txt" <<EOF
Installed: $(date -Is)
Version: U2.52.3
Core SHA256: $(sha256sum "$ACTIVE_CORE" | awk '{print $1}')
Daemon SHA256: $(sha256sum "$ACTIVE_DAEMON" | awk '{print $1}')
SP404 daemon: ${HOST_BACKENDS:-0}
MIDI daemon: ${HOST_BACKENDS:-0}
Host USB audio modules: ${HOST_MODULES:-0}
Backup: $BACKUP
EOF
sync
echo INSTALL_U2523_OK
echo "BACKUP=$BACKUP"
