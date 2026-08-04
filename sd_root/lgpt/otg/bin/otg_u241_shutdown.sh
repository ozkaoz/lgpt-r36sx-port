#!/bin/sh
BASE=/mnt/sdcard/lgpt/otg
LOGROOT=/mnt/sdcard/LGPT_OTG_LOGS
mkdir -p "$LOGROOT" 2>/dev/null || true
exec >> "$LOGROOT/U2517_SHUTDOWN.log" 2>&1

date
# v14: stop every USB audio daemon (Windows UAC2, SP404 host, USB-MIDI)
# BEFORE any musb role operation. Leaving the SP404 host stream live while
# switch_host_role re-drives the controller reset the device mid-stream and
# panicked the console (crash al volver a Sampler).
killall r36s_u241_usb_audio_io 2>/dev/null || true
killall r36s_sp404_host_audio_io 2>/dev/null || true
killall r36s_midi_host_io 2>/dev/null || true
rm -f /tmp/r36sx_uac2_bridge_fifo \
      /tmp/r36sx_usb_capture_monitor_fifo \
      /tmp/r36sx_lgpt_usb/daemon_pid

for G in /sys/kernel/config/usb_gadget/r36sx_lgpt_*; do
    [ -d "$G" ] && echo "" > "$G/UDC" 2>/dev/null || true
done

echo "U2517_SHUTDOWN_DONE"
