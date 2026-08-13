#!/bin/sh
set -u

BASE=/mnt/sdcard/lgpt/otg
BIN=$BASE/bin
# SD lifecycle U2.54: all runtime logs on tmpfs; clean shutdown flushes them.
LOGROOT="/tmp/r36sx_lgpt_logs"
INTERNAL_LOG="/tmp/r36sx_lgpt_logs/mirror"
RUNTIME=/tmp/r36sx_lgpt_usb
FIFO=/tmp/r36sx_uac2_bridge_fifo
MONITOR_FIFO=/tmp/r36sx_usb_capture_monitor_fifo
DAEMON=$BIN/r36s_u241_usb_audio_io
COMMON=$BIN/otg_u241_common.sh
LOG=$LOGROOT/U2517_AUDIO_DRIVER_SETUP.log
GADGET=/sys/kernel/config/usb_gadget/r36sx_lgpt_u2414
UDC_NAME=musb-hdrc.0.auto

mkdir -p "$LOGROOT" "$INTERNAL_LOG" "$INTERNAL_LOG/runtime_state" "$RUNTIME" 2>/dev/null || true

LOCK=/tmp/r36sx_u2414_audio_driver_lock
if ! mkdir "$LOCK" 2>/dev/null; then
    echo "U2517_SETUP_ALREADY_RUNNING" >> "$LOG"
    exit 0
fi
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT INT TERM

if [ -f "$LOG" ] && [ "$(wc -c < "$LOG" 2>/dev/null || echo 0)" -gt 1048576 ]; then
    mv -f "$LOG" "$LOG.previous" 2>/dev/null || true
fi
# v14.1: RO-proof logging - if the SD FAT is mounted read-only (dirty bit
# from a bad unplug) the exec redirect would abort the whole setup. Fall back
# to /tmp so the Windows gadget can still be configured and its daemon start.
if ! ( : >> "$LOG" ) 2>/dev/null; then
    exec >> /tmp/u2517_audio_driver_setup.log 2>&1
else
    exec >> "$LOG" 2>&1
fi

echo "R36SX LGPT U2.51.7 ABI7 MONITOR-FIFO HANDSHAKE OTG SETUP"
date

[ -f "$BASE/enable_lgpt_uac2_bridge" ] || {
    echo "OTG_DISABLED_NO_SENTINEL"
    exit 0
}
[ -f "$COMMON" ] || {
    echo "ERROR_COMMON_MISSING=$COMMON"
    exit 2
}
. "$COMMON"

REQUESTED_PROFILE="$(cat "$BASE/audio_usb_profile" 2>/dev/null || true)"
case "$REQUESTED_PROFILE" in
    STEREO_48K)
        EXPECTED_CHANNELS=2
        EXPECTED_CHMASK=3
        ;;
    *)
        REQUESTED_PROFILE=MONO_48K
        EXPECTED_CHANNELS=1
        EXPECTED_CHMASK=1
        ;;
esac

write_setup_result() {
    text="$1"
    printf '%s\n' "$text" > "$RUNTIME/setup_result" 2>/dev/null || true
    printf '%s\n' "$text" > "$INTERNAL_LOG/runtime_state/setup_result" 2>/dev/null || true
}

ensure_monitor_fifo() {
    if [ -e "$MONITOR_FIFO" ] && [ ! -p "$MONITOR_FIFO" ]; then
        rm -f "$MONITOR_FIFO" || return 1
    fi
    if [ ! -p "$MONITOR_FIFO" ]; then
        mkfifo "$MONITOR_FIFO" || return 1
    fi
    chmod 666 "$MONITOR_FIFO" 2>/dev/null || true
    echo "U2517_MONITOR_FIFO_READY=$MONITOR_FIFO"
    return 0
}

runtime_contract_matches() {
    [ -d "$GADGET" ] || return 1
    [ "$(cat "$GADGET/UDC" 2>/dev/null)" = "$UDC_NAME" ] || return 1
    [ -n "$OLDPID" ] || return 1
    kill -0 "$OLDPID" 2>/dev/null || return 1
    [ -p "$FIFO" ] || return 1
    grep -q 'R36SX_USB_AUDIO_DAEMON_ABI=7' \
        "$RUNTIME/daemon_version" 2>/dev/null || return 1
    grep -q 'R36SX_CAPTURE_ABI=2' \
        "$RUNTIME/capture_abi" 2>/dev/null || return 1
    [ "$(cat "$RUNTIME/audio_channels" 2>/dev/null)" = "$EXPECTED_CHANNELS" ] || return 1
    [ "$(cat "$RUNTIME/audio_rate" 2>/dev/null)" = "48000" ] || return 1
    [ "$(cat "$RUNTIME/audio_profile" 2>/dev/null)" = "$REQUESTED_PROFILE" ] || return 1
    return 0
}

gadget_profile_matches() {
    [ -d "$GADGET/functions/uac2.usb0" ] || return 1
    [ "$(cat "$GADGET/UDC" 2>/dev/null)" = "$UDC_NAME" ] || return 1
    [ "$(cat "$GADGET/functions/uac2.usb0/p_chmask" 2>/dev/null)" = "$EXPECTED_CHMASK" ] || return 1
    [ "$(cat "$GADGET/functions/uac2.usb0/c_chmask" 2>/dev/null)" = "$EXPECTED_CHMASK" ] || return 1
    [ "$(cat "$GADGET/functions/uac2.usb0/p_srate" 2>/dev/null)" = "48000" ] || return 1
    [ "$(cat "$GADGET/functions/uac2.usb0/c_srate" 2>/dev/null)" = "48000" ] || return 1
    [ -e /dev/snd/pcmC0D0p ] || return 1
    [ -e /dev/snd/pcmC0D0c ] || return 1
    return 0
}

start_daemon_only() {
    recovery_kind="$1"

    [ -p "$FIFO" ] || {
        rm -f "$FIFO"
        mkfifo "$FIFO" || return 5
        chmod 666 "$FIFO" 2>/dev/null || true
    }
    ensure_monitor_fifo || return 9

    printf 'USB capture idle\n' > "$RUNTIME/usb_capture_status"
    printf '0\n' > "$RUNTIME/usb_capture_monitor"
    printf 'U2517_SETUP_STARTING\n' > "$RUNTIME/daemon_version"
    printf '%s\n' "$REQUESTED_PROFILE" > "$RUNTIME/audio_profile"
    printf '%s\n' "$EXPECTED_CHANNELS" > "$RUNTIME/audio_channels"
    printf '48000\n' > "$RUNTIME/audio_rate"

    DLOG=$LOGROOT/U2517_USB_AUDIO_DAEMON.log
    if [ -f "$DLOG" ] && [ "$(wc -c < "$DLOG" 2>/dev/null || echo 0)" -gt 8388608 ]; then
        mv -f "$DLOG" "$DLOG.previous" 2>/dev/null || true
    fi
    # v14.1: RO-proof launch (dirty SD FAT mounted read-only would fail the
    # redirect and the Windows daemon would never start).
    if ( : >> "$DLOG" ) 2>/dev/null; then
        DLOG_TARGET="$DLOG"
    else
        DLOG_TARGET=/tmp/u2517_usb_audio_daemon.log
    fi

    "$DAEMON" "$FIFO" /dev/snd/pcmC0D0p /dev/snd/pcmC0D0c "$EXPECTED_CHANNELS" \
        >> "$DLOG_TARGET" 2>&1 &
    daemon_pid=$!
    printf '%s\n' "$daemon_pid" > "$RUNTIME/daemon_pid"
    echo "DAEMON_PID=$daemon_pid RECOVERY_KIND=$recovery_kind"

    sleep 1
    kill -0 "$daemon_pid" 2>/dev/null || {
        write_setup_result "error daemon-exited-early recovery=$recovery_kind"
        return 6
    }
    grep -q 'R36SX_USB_AUDIO_DAEMON_ABI=7' \
        "$RUNTIME/daemon_version" 2>/dev/null || {
        write_setup_result "error daemon-version-contract recovery=$recovery_kind"
        return 7
    }
    [ "$(cat "$RUNTIME/audio_channels" 2>/dev/null)" = "$EXPECTED_CHANNELS" ] || {
        write_setup_result "error channel-contract expected=$EXPECTED_CHANNELS"
        return 8
    }

    write_setup_result "ready-$recovery_kind profile=$REQUESTED_PROFILE channels=$EXPECTED_CHANNELS pid=$daemon_pid"
    echo "U2517_DAEMON_READY recovery=$recovery_kind profile=$REQUESTED_PROFILE channels=$EXPECTED_CHANNELS pid=$daemon_pid"
    return 0
}

OLDPID="$(cat "$RUNTIME/daemon_pid" 2>/dev/null || true)"
if runtime_contract_matches; then
    echo "U2517_ALREADY_READY_ABI7 PID=$OLDPID UDC=$UDC_NAME PROFILE=$REQUESTED_PROFILE CHANNELS=$EXPECTED_CHANNELS"
    printf '%s\n' "ready-existing profile=$REQUESTED_PROFILE channels=$EXPECTED_CHANNELS pid=$OLDPID" \
        > "$RUNTIME/setup_result" 2>/dev/null || true
    exit 0
fi

# U2.51.7: if only the userspace daemon died, do not unbind the gadget or
# unload the UAC2 module. Restarting in place preserves the Windows endpoint.
if gadget_profile_matches; then
    echo "U2517_DAEMON_ONLY_RECOVERY profile=$REQUESTED_PROFILE channels=$EXPECTED_CHANNELS"
    for p in r36s_u241_usb_audio_io r36s_u240_usb_audio_io r36s_au11_usb_audio_io; do
        pidof "$p" >/dev/null 2>&1 && killall "$p" 2>/dev/null || true
    done
    rm -f "$RUNTIME/daemon_pid"
    start_daemon_only daemon-only || {
        rc=$?
        echo "ERROR_DAEMON_ONLY_RECOVERY_RC=$rc"
        exit "$rc"
    }
    echo "U2517_ABI7_DAEMON_ONLY_RECOVERY_OK"
    exit 0
fi

set -x
echo "U2517_RUNTIME_REBUILD_REQUIRED requested_profile=$REQUESTED_PROFILE expected_channels=$EXPECTED_CHANNELS oldpid=${OLDPID:-none}"
write_setup_result "rebuilding profile=$REQUESTED_PROFILE channels=$EXPECTED_CHANNELS"

for p in r36s_u241_usb_audio_io r36s_u240_usb_audio_io r36s_au11_usb_audio_io; do
    pidof "$p" >/dev/null 2>&1 && killall "$p" 2>/dev/null || true
done
rm -f "$RUNTIME/daemon_pid"
sleep 1

u2414_cleanup_gadgets
UNLOAD_OK=0
for attempt in 1 2 3 4 5; do
    if u2414_unload_uac2; then
        UNLOAD_OK=1
        break
    fi
    echo "U2517_UNLOAD_RETRY attempt=$attempt"
    sleep 1
done
[ "$UNLOAD_OK" -eq 1 ] || {
    echo "ERROR_UNLOAD_PREVIOUS_UAC2_AFTER_RETRIES=YES"
    u2414_snapshot
    exit 4
}

u2414_load_stack || {
    rc=$?
    echo "ERROR_LOAD_AU8_SYNC_STACK_RC=$rc"
    u2414_snapshot
    exit "$rc"
}
u2414_create_gadget || {
    rc=$?
    echo "ERROR_CREATE_GADGET_RC=$rc"
    u2414_snapshot
    exit "$rc"
}
u2414_bind || {
    rc=$?
    echo "ERROR_BIND_GADGET_RC=$rc"
    u2414_snapshot
    exit "$rc"
}
u2414_wait_alsa || {
    rc=$?
    echo "ERROR_WAIT_ALSA_RC=$rc"
    u2414_snapshot
    exit "$rc"
}

rm -f "$FIFO"
mkfifo "$FIFO" || {
    echo "ERROR_FIFO_CREATE=YES"
    u2414_snapshot
    exit 5
}
chmod 666 "$FIFO" 2>/dev/null || true
ensure_monitor_fifo || {
    echo "ERROR_MONITOR_FIFO_CREATE=YES"
    u2414_snapshot
    exit 9
}

start_daemon_only full-rebuild || {
    rc=$?
    echo "ERROR_DAEMON_START_RC=$rc"
    u2414_snapshot
    exit "$rc"
}

u2414_snapshot
cp -f "$LOG" "$INTERNAL_LOG/U2517_AUDIO_DRIVER_SETUP.log" 2>/dev/null || true

echo "U2517_ABI7_MONITOR_FIFO_HANDSHAKE_SETUP_OK"
exit 0
