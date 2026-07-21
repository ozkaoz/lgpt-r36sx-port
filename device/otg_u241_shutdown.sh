#!/bin/sh
BASE=/mnt/sdcard/lgpt/otg
LOGROOT=/mnt/sdcard/LGPT_OTG_LOGS
mkdir -p "$LOGROOT" 2>/dev/null || true
exec >> "$LOGROOT/U2517_SHUTDOWN.log" 2>&1

date
killall r36s_u241_usb_audio_io 2>/dev/null || true
rm -f /tmp/r36sx_uac2_bridge_fifo \
      /tmp/r36sx_usb_capture_monitor_fifo \
      /tmp/r36sx_lgpt_usb/daemon_pid

for G in /sys/kernel/config/usb_gadget/r36sx_lgpt_*; do
    [ -d "$G" ] && echo "" > "$G/UDC" 2>/dev/null || true
done

echo "U2517_SHUTDOWN_DONE"
