#!/bin/sh
BASE=/mnt/sdcard/lgpt/otg
# SD lifecycle U2.54: the shutdown runner itself logs to tmpfs too; at the end
# it flushes the entire /tmp/r36sx_lgpt_logs tree to the card once (+ sync).
LOGROOT=/tmp/r36sx_lgpt_logs
mkdir -p "$LOGROOT" 2>/dev/null || true
# v14.1: RO-proof logging - if the SD log cannot be opened (dirty FAT
# mounted read-only) fall back to /tmp so the shutdown always runs before
# role operations.
if ! ( : >> "$LOGROOT/U2517_SHUTDOWN.log" ) 2>/dev/null; then
    exec >> /tmp/u2517_shutdown.log 2>&1
else
    exec >> "$LOGROOT/U2517_SHUTDOWN.log" 2>&1
fi

date
# v14.3: graceful daemon stop. A plain `killall`/SIGTERM to a daemon with live
# usb-audio URBs in flight lets the kernel close the fds from under the
# blocked ALSA ioctl, which wedges the musb host controller on this SoC and
# panics the console (crash al volver a Sampler). Signal SIGUSR1 first so the
# daemon drains and closes its PCMs from its own loop, then force only if it
# does not exit.
stop_daemon() {
    name="$1"
    pidof "$name" >/dev/null 2>&1 || return 0
    pkill -USR1 -x "$name" 2>/dev/null || pkill -USR1 "$name" 2>/dev/null || true
    n=0
    while pidof "$name" >/dev/null 2>&1 && [ "$n" -lt 30 ]; do
        sleep 0.1
        n=$((n + 1))
    done
    if pidof "$name" >/dev/null 2>&1; then
        killall -9 "$name" 2>/dev/null || true
    fi
}
stop_daemon r36s_u241_usb_audio_io
stop_daemon r36s_sp404_host_audio_io
stop_daemon r36s_midi_host_io

# U2.55b: flush the RAM log tree BEFORE the UDC unbind. The musb gadget
# teardown below can wedge the host controller and panic this SoC, which
# previously discarded the whole session's logs before they reached the SD.
if [ -f "$BASE/bin/otg_u241_common.sh" ]; then
    . "$BASE/bin/otg_u241_common.sh"
fi
if command -v u2414_flush_logs_to_sd >/dev/null 2>&1; then
    u2414_flush_logs_to_sd
fi

rm -f /tmp/r36sx_uac2_bridge_fifo \
      /tmp/r36sx_usb_capture_monitor_fifo \
      /tmp/r36sx_lgpt_usb/daemon_pid

for G in /sys/kernel/config/usb_gadget/r36sx_lgpt_*; do
    [ -d "$G" ] && echo "" > "$G/UDC" 2>/dev/null || true
done

echo "U2517_SHUTDOWN_DONE"
sync

# U2.54: if the flush above skipped (SD temporarily busy/read-only), retry it
# once after teardown when the system is quiet.
if [ -f "$BASE/bin/otg_u241_common.sh" ]; then
    . "$BASE/bin/otg_u241_common.sh"
fi
if command -v u2414_flush_logs_to_sd >/dev/null 2>&1; then
    u2414_flush_logs_to_sd
fi
sync

echo "U2517_SHUTDOWN_FLUSH_DONE"
