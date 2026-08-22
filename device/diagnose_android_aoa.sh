#!/bin/sh
set -u
BASE=/mnt/sdcard/lgpt/otg
BIN=$BASE/bin
LOGROOT=/mnt/sdcard/LGPT_OTG_LOGS
RUNTIME=/tmp/r36sx_lgpt_usb
GADGET=/sys/kernel/config/usb_gadget/r36sx_lgpt_u2414
H35_ROLE_PATH="/sys/devices/platform/soc/18844000.usb/musb-hdrc.0.auto/mode"
[ -e "$H35_ROLE_PATH" ] || H35_ROLE_PATH="$(find /sys/devices -path '*musb-hdrc.0.auto/mode' -print -quit 2>/dev/null)"
TIMESTAMP="$(date +%Y%m%d_%H%M%S 2>/dev/null || echo no-date)"
OUT="$LOGROOT/ANDROID_DIAGNOSTIC_$TIMESTAMP.txt"
mkdir -p "$LOGROOT" 2>/dev/null || true
{
echo "timestamp=$TIMESTAMP"
echo "HEAD=$(cat /home/dafunknoise/lgpt-repo/.git/refs/heads/feature/bacon-1.5-fx 2>/dev/null || git -C /home/dafunknoise/lgpt-repo rev-parse HEAD 2>/dev/null || echo unknown)"
echo "version=$(cat /home/dafunknoise/lgpt-repo/VERSION 2>/dev/null || cat /mnt/sdcard/lgpt/VERSION 2>/dev/null || echo unknown)"
echo "audio_driver_mode=$(cat $RUNTIME/audio_driver_mode 2>/dev/null || cat $BASE/audio_driver_mode 2>/dev/null || echo none)"
echo "audio_driver_policy=$(cat $RUNTIME/audio_driver_policy 2>/dev/null || cat $BASE/audio_driver_policy 2>/dev/null || echo none)"
echo "generation=$(cat $RUNTIME/h35_android_generation 2>/dev/null || echo 0)"
echo "u241_setup_pid=$(cat /tmp/r36sx_lgpt_usb/u241_setup_pid 2>/dev/null || echo none)"
echo "u241_setup_pid_alive=$(kill -0 $(cat /tmp/r36sx_lgpt_usb/u241_setup_pid 2>/dev/null) 2>/dev/null && echo yes || echo no)"
echo "u241_lock=$(ls -ld /tmp/r36sx_u2414_audio_driver_lock 2>&1 | head -n 1)"
echo "u241_daemon_pids=$(pidof r36s_u241_usb_audio_io 2>/dev/null || echo none)"
echo "UDC_BEFORE=$(cat $GADGET/UDC 2>/dev/null || echo none)"
echo "MUSB_ROLE_BEFORE=$(cat $H35_ROLE_PATH 2>/dev/null || echo none)"
echo "ANDROID_APPLY_START=$(date)"
echo "u241_setup_pid_after_stop=$(cat /tmp/r36sx_lgpt_usb/u241_setup_pid 2>/dev/null || echo none)"
echo "u241_lock_after_stop=$(ls -ld /tmp/r36sx_u2414_audio_driver_lock 2>&1 | head -n 1)"
echo "UDC_AFTER_WINDOWS_SHUTDOWN=$(cat $GADGET/UDC 2>/dev/null || echo none)"
for ms in 0 100 250 500 1000; do
  echo "MUSB_ROLE_${ms}MS=$(cat $H35_ROLE_PATH 2>/dev/null || echo none)"
  [ "$ms" != "0" ] && sleep 0.1
done
echo "android_supervisor_pid=$(cat $RUNTIME/h35_android_supervisor_pid 2>/dev/null || echo none)"
echo "aoa_audio_daemon_pid=$(cat $RUNTIME/daemon_pid 2>/dev/null || echo none)"
echo "aoa_receiver_pid=$(cat $RUNTIME/h35_android_receiver_pid 2>/dev/null || cat $RUNTIME/bulk_receiver_pid 2>/dev/null || echo none)"
echo "USB devices before phone:"
ls /sys/bus/usb/devices/ 2>/dev/null | head -n 20
for dev in /sys/bus/usb/devices/*; do
  [ -e "$dev/idVendor" ] || continue
  echo "dev=$(basename $dev) vid=$(cat $dev/idVendor 2>/dev/null) pid=$(cat $dev/idProduct 2>/dev/null) manuf=$(cat $dev/manufacturer 2>/dev/null | head -c 40) prod=$(cat $dev/product 2>/dev/null | head -c 40) serial=$(cat $dev/serial 2>/dev/null | head -c 40)"
done
echo "USB devices after phone:"
sleep 1
ls /sys/bus/usb/devices/ 2>/dev/null | head -n 20
echo "AOA state=$(cat $RUNTIME/aoa_state 2>/dev/null || echo none)"
echo "AOA result=$(cat $RUNTIME/aoa_result 2>/dev/null || echo none)"
echo "AOA protocol=$(cat $RUNTIME/aoa_protocol 2>/dev/null || echo none)"
echo "AOA accessory present=$(cat $RUNTIME/aoa_bulk_accessory_present 2>/dev/null || echo none)"
echo "daemon logs:"
cat $LOGROOT/U2517_USB_AUDIO_DAEMON.log 2>/dev/null | tail -n 20
echo "receiver logs:"
cat $LOGROOT/*aoa* 2>/dev/null | tail -n 20
echo "dmesg tail:"
dmesg 2>/dev/null | tail -n 30
} > "$OUT" 2>&1
echo "ANDROID_DIAGNOSTIC_WRITTEN=$OUT"
cat "$OUT"
