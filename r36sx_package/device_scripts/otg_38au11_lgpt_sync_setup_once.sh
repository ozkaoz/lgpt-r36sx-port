#!/bin/sh
set -u
LOGDIR=/mnt/sdcard/lgpt/otg/logs
mkdir -p "$LOGDIR" 2>/dev/null || true
LOG="$LOGDIR/u2_38au11_lgpt_sync_setup_once.log"
if ! { : >>"$LOG"; } 2>/dev/null; then LOG=/tmp/u2_38au11_lgpt_sync_setup_once.log; fi
exec >>"$LOG" 2>&1
set -x
echo "U2.38AU11Z LGPT USB sampler setup: cleanroom AU11U stable + AU10Y OTG descriptor"
date
COMMON=/mnt/sdcard/lgpt/otg/bin/otg_38au11_common.sh
DAEMON=/mnt/sdcard/lgpt/otg/bin/r36s_au11_usb_audio_io
FIFO=/tmp/r36sx_uac2_bridge_fifo
RUNTIME=/tmp/r36sx_lgpt_usb
PCM_PLAY=/dev/snd/pcmC0D0p
PCM_CAP=/dev/snd/pcmC0D0c
MODE=/mnt/sdcard/lgpt/otg/audio_driver_mode
if [ ! -f /mnt/sdcard/lgpt/otg/enable_lgpt_uac2_bridge ]; then echo DISABLED_NO_SENTINEL; exit 0; fi
[ -f "$MODE" ] || echo USB_OUT_AUTO_MUTE > "$MODE" 2>/dev/null || true
[ -f "$COMMON" ] || { echo ERROR_COMMON_MISSING="$COMMON"; exit 2; }
. "$COMMON"
load_audio_stack_au11 sync || echo WARN_LOAD_STACK_RC=$?
if [ ! -d /sys/kernel/config/usb_gadget/r36sx_uac2_au11_duplex ]; then
  create_uac2_legacy_duplex_gadget_au11 musb-hdrc.0.auto duplex_stable_always_open || echo WARN_CREATE_DUPLEX_AU11Z_RC=$?
else
  echo GADGET_ALREADY_EXISTS=/sys/kernel/config/usb_gadget/r36sx_uac2_au11_duplex PROFILE=$(cat /tmp/r36sx_au11_active_profile 2>/dev/null)
  print_udc_status_au11
fi
mkdir -p /mnt/sdcard/lgpt/otg /mnt/sdcard/lgpt/otg/logs "$RUNTIME" 2>/dev/null || true
DAEMON_VERSION_FILE="$RUNTIME/daemon_version"
if [ -p "$FIFO" ] && pidof r36s_au11_usb_audio_io >/dev/null 2>&1 && [ "$(cat "$DAEMON_VERSION_FILE" 2>/dev/null)" = "AU11Z" ]; then
  echo "AU11Z_IDEMPOTENT_SETUP_KEEP_RUNNING_DAEMON pid=$(pidof r36s_au11_usb_audio_io) fifo=$FIFO"
  print_udc_status_au11
  exit 0
fi
rm -f "$FIFO"
mkfifo "$FIFO" || true
chmod 666 "$FIFO" || true
touch "$RUNTIME/usb_capture_status" 2>/dev/null || true
printf "USB capture idle
" > "$RUNTIME/usb_capture_status" 2>/dev/null || true
printf "0
" > "$RUNTIME/usb_capture_monitor" 2>/dev/null || true
printf "AU11Z
" > "$DAEMON_VERSION_FILE" 2>/dev/null || true
# Kill every stale daemon/manager from AU9/AU10/AU11 before starting the single current bridge.
for p in r36s_au11_usb_audio_io $(ls /mnt/sdcard/lgpt/otg/bin/r36s_au10*_usb_audio_io 2>/dev/null | xargs -n1 basename 2>/dev/null) $(ls /mnt/sdcard/lgpt/otg/bin/r36s_au9*_fifo_to_uac2 2>/dev/null | xargs -n1 basename 2>/dev/null); do
  [ -n "$p" ] || continue
  pidof "$p" >/dev/null 2>&1 && killall "$p" 2>/dev/null || true
done
for m in $(ls /mnt/sdcard/lgpt/otg/bin/otg_38au10*_mode_manager.sh 2>/dev/null | xargs -n1 basename 2>/dev/null) otg_38au11_mode_manager.sh; do
  [ -n "$m" ] || continue
  killall "$m" 2>/dev/null || true
done
[ -x "$DAEMON" ] || { echo ERROR_DAEMON_MISSING="$DAEMON"; exit 3; }
DAEMON_LOG="$LOGDIR/u2_38au11_usb_audio_io_daemon.log"
if ! { : >>"$DAEMON_LOG"; } 2>/dev/null; then DAEMON_LOG=/tmp/u2_38au11_usb_audio_io_daemon.log; fi
"$DAEMON" "$FIFO" "$PCM_PLAY" "$PCM_CAP" >>"$DAEMON_LOG" 2>&1 &
echo AU11Z_DAEMON_START_REQUESTED=YES
printf "AU11Z
" > "$DAEMON_VERSION_FILE" 2>/dev/null || true
echo DAEMON_PID=$!
echo DAEMON_LOG=$DAEMON_LOG
sleep 2
ls -lah /dev/snd || true
cat /proc/asound/cards || true
cat /proc/asound/pcm || true
cat "$MODE" || true
cat "$RUNTIME/usb_capture_status" 2>/dev/null || true
for u in /sys/class/udc/*; do [ -e "$u/state" ] && echo UDC=$(basename "$u") STATE=$(cat "$u/state") SPEED=$(cat "$u/current_speed" 2>/dev/null); done
exit 0
