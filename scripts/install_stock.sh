#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/Toolchains/R36SX}"
SD="${SD_MOUNT:-/mnt/f}"
CORE="$PROJECT_ROOT/BUILD/U2523/lgpt_r36sx_u2523.so"
DAEMON="$PROJECT_ROOT/BUILD/U2523/r36s_u2523_usb_audio_io"
[[ -s "$SD/cubegm/picoarch" ]] || { echo "ERROR: TreeFrogUI is not installed" >&2; exit 1; }
[[ -s "$CORE" && -s "$DAEMON" ]] || { echo "ERROR: build U2.52.3 first" >&2; exit 1; }
mkdir -p "$SD/cubegm/cores" "$SD/roms/lgpt" "$SD/lgpt/samples/records" "$SD/lgpt/instruments" "$SD/lgpt/projects" "$SD/lgpt/tmp/record" "$SD/lgpt/otg/bin" "$SD/lgpt/otg/logs/runtime_state" "$SD/lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2" "$SD/LGPT_OTG_LOGS"
install -m 0755 "$ROOT/device/lgpt_launcher_u241.sh" "$SD/cubegm/lgpt"
install -m 0755 "$CORE" "$SD/cubegm/cores/lgpt_r36sx_port_libretro.so"
install -m 0755 "$DAEMON" "$SD/lgpt/otg/bin/r36s_u241_usb_audio_io"
cp -f "$ROOT/deployment/start.lgpt" "$SD/roms/lgpt/start.lgpt"
cp -f "$ROOT/deployment/config.stock.xml" "$SD/lgpt/config.xml"
# U2.56.1 ALSA_STACK: the UAC2 gadget needs the port's ALSA core modules
# (soundcore/snd/snd-timer/snd-pcm) next to usb_f_uac2.ko - otg_u241_common.sh
# probes every module under modules/4.4.186-release and falls back to the
# host_usb_audio copies, which are built for a different kernel and fail with
# "unknown symbol". Without this stack the gadget is never created and the PC
# does not detect the console (U2517 setup: snd.ko unknown symbols).
for m in soundcore.ko snd.ko snd-timer.ko snd-pcm.ko usb_f_uac2.ko; do
  install -m 0644 "$ROOT/recovery/u2_38au8_sync_uac2/$m" "$SD/lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2/$m"
done
for f in otg_u241_common.sh otg_u241_setup_once.sh otg_u241_apply_profile_once.sh otg_u241_shutdown.sh; do install -m 0755 "$ROOT/device/$f" "$SD/lgpt/otg/bin/$f"; done
: > "$SD/lgpt/otg/enable_lgpt_uac2_bridge"
printf 'STEREO_48K\n' > "$SD/lgpt/otg/audio_usb_profile"
printf 'LOCAL_CONSOLE\n' > "$SD/lgpt/otg/audio_driver_mode"
rm -f "$SD/lgpt/otg/lowlat_240" "$SD/lgpt/last_project"
sync
echo INSTALL_STOCK_U2523_OK
