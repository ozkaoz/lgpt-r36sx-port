#!/bin/sh
# H38.2 3-mode apply script. Forked by the core (and by TreeFrogUI) with the
# mode token. LOCAL_CONSOLE tears everything down, WINDOWS runs the ABI7
# gadget/daemon setup from the GitHub repo payload, ANDROID switches the USB
# controller to host role and starts the AOA supervisor with the h36 daemons.
set -u
BASE=/mnt/sdcard/lgpt/otg
BIN=$BASE/bin
LOGROOT=/mnt/sdcard/LGPT_OTG_LOGS
RUNTIME=/tmp/r36sx_lgpt_usb
mkdir -p "$LOGROOT" "$RUNTIME" 2>/dev/null || true

MODE="${1:-LOCAL_CONSOLE}"

normalize_mode() {
    case "$1" in
        ANDROID|ANDROID_OTG|ANDROID_AOA|USB_IN|USB_IN_OTG) echo ANDROID ;;
        USB_OUT|USB_OUT_OTG|SP404|SP404_OTG) echo USB_OUT ;;
        MIDI|MIDI_OTG) echo MIDI ;;
        WINDOWS|WINDOWS_OTG|USB_DUPLEX|USB_DUPLEX_OTG|USB_IN_OUT|USB_INPUT_OUTPUT|FULL_DUPLEX|USB_OUT_AUTO_MUTE|USB_IN_CAPTURE|EXTERNAL_RECORD) echo WINDOWS ;;
        *) echo LOCAL_CONSOLE ;;
    esac
}
NORM="$(normalize_mode "$MODE")"

HOST_MODBASE=$BASE/modules/4.4.186-release/host_usb_audio
# ALSA core stack must be resident before snd-usbmidi-lib/snd-usb-audio load;
# otherwise insmod fails with unresolved snd_*/snd_pcm_*/snd_rawmidi symbols.
HOST_CORE_MODULES="soundcore.ko snd.ko snd-timer.ko snd-pcm.ko snd-hwdep.ko snd-seq-device.ko snd-rawmidi.ko"
HOST_MODULES="snd-usbmidi-lib.ko snd-usb-audio.ko"

u2414_loaded() {
    grep -q "^$1 " /proc/modules 2>/dev/null
}

load_host_usb_module() {
    filename="$1"
    module="$(echo "${filename%.ko}" | tr '-' '_')"
    if u2414_loaded "$module"; then
        echo "HOST_LOAD_${filename}_ALREADY=YES"
        return 0
    fi
    if command -v modprobe >/dev/null 2>&1; then
        modprobe "$module" 2>/dev/null && {
            echo "HOST_LOAD_${filename}_MODPROBE=YES"
            return 0
        }
    fi
    for p in \
      /lib/modules/4.4.186-release/kernel/sound/core/"$filename" \
      /lib/modules/4.4.186-release/kernel/sound/usb/"$filename" \
      /lib32/modules/4.4.186-release/kernel/sound/core/"$filename" \
      /lib32/modules/4.4.186-release/kernel/sound/usb/"$filename" \
      "$BASE"/modules/4.4.186-release/u2_38au8_sync_uac2/"$filename" \
      $(find "$HOST_MODBASE" -type f -name "$filename" 2>/dev/null); do
        [ -f "$p" ] || continue
        insmod "$p" 2>>"$LOGROOT/H38_HOST_MODULE_LOAD.err" && {
            echo "HOST_LOAD_${filename}_INSMOD=YES FROM=$p"
            return 0
        }
        echo "insmod $p -> rc=$?" >>"$LOGROOT/H38_HOST_MODULE_LOAD.err" 2>/dev/null || true
    done
    echo "HOST_LOAD_${filename}_FAILED=YES"
    return 1
}

load_host_usb_modules() {
    failed=""
    for m in $HOST_CORE_MODULES $HOST_MODULES; do
        load_host_usb_module "$m" || failed="$failed $m"
    done
    if [ -n "$failed" ]; then
        {
            echo "H38_HOST_STACK_ABORT failed=[$failed]"
            echo "  dmesg tail:"
            dmesg 2>/dev/null | tail -n 60
        } >>"$LOGROOT/H38_HOST_MODULE_LOAD.err" 2>/dev/null || true
        return 1
    fi
    return 0
}

H35_ROLE_PATH="/sys/devices/platform/soc/18844000.usb/musb-hdrc.0.auto/mode"
# v14.2: a host supervisor left over from USB_OUT/SP404/MIDI keeps respawning
# the SP404/MIDI daemons while they hold the host PCM - and, worst case, it
# respawned them DURING a WINDOWS apply (policy USB_DUPLEX_OTG used to count
# as a host policy), racing the musb gadget rebuild with HW_PARAMS EIO storms
# and role flips that crashed the console. Force-stop the supervisor + daemons
# before ANY gadget/role operation.
stop_host_supervisor() {
    s="$(cat "$RUNTIME/h38_host_supervisor_pid" 2>/dev/null || true)"
    if [ -n "$s" ] && kill -0 "$s" 2>/dev/null; then
        echo "HOST_SUPERVISOR_STOP pid=$s"
        kill "$s" 2>/dev/null || true
        n=0
        while kill -0 "$s" 2>/dev/null && [ "$n" -lt 20 ]; do sleep 0.1; n=$((n+1)); done
        kill -0 "$s" 2>/dev/null && kill -9 "$s" 2>/dev/null || true
    fi
    for p in r36s_sp404_host_audio_io r36s_midi_host_io; do
        pidof "$p" >/dev/null 2>&1 && killall "$p" 2>/dev/null || true
    done
}
switch_host_role() {
    for g in /sys/kernel/config/usb_gadget/r36sx_lgpt_* \
             /sys/kernel/config/usb_gadget/r36sx_uac2_*; do
        [ -d "$g" ] || continue
        echo "" > "$g/UDC" 2>/dev/null || true
    done
    [ -e "$H35_ROLE_PATH" ] || H35_ROLE_PATH="$(find /sys/devices -path '*musb-hdrc.0.auto/mode' -print -quit 2>/dev/null)"
    [ -n "$H35_ROLE_PATH" ] && [ -e "$H35_ROLE_PATH" ] || {
        echo "ERROR_HOST_ROLE_PATH_MISSING=YES"
        return 2
    }
    echo "ROLE_BEFORE $H35_ROLE_PATH=$(cat "$H35_ROLE_PATH" 2>/dev/null)"
    # v14: if the controller is already in host role, do not re-drive it.
    # Re-writing mode on an already-host MUSB reset the SP404 endpoint while
    # the daemon streamed, panicking the console on the Sampler bounce.
    case "$(cat "$H35_ROLE_PATH" 2>/dev/null)" in
        host|b_host)
            echo "ROLE_ALREADY_HOST skip_rewrite=1"
            return 0
            ;;
    esac
    echo host > "$H35_ROLE_PATH" 2>/dev/null || echo b_host > "$H35_ROLE_PATH" 2>/dev/null || {
        echo "ERROR_HOST_ROLE_WRITE_FAILED=YES"
        return 3
    }
    echo "ROLE_AFTER $H35_ROLE_PATH=$(cat "$H35_ROLE_PATH" 2>/dev/null)"
    return 0
}

case "$NORM" in
    ANDROID) POLICY=USB_IN_OTG ;;
    WINDOWS) POLICY=USB_DUPLEX_OTG ;;
    USB_OUT) POLICY=USB_OUT_OTG ;;
    MIDI) POLICY=MIDI_OTG ;;
    *) POLICY=LOCAL_CONSOLE ;;
esac

echo "$NORM" > "$BASE/audio_driver_mode" 2>/dev/null || true
echo "$NORM" > "$RUNTIME/audio_driver_mode" 2>/dev/null || true
echo "$POLICY" > "$BASE/audio_driver_policy" 2>/dev/null || true
echo "$POLICY" > "$RUNTIME/audio_driver_policy" 2>/dev/null || true

{
    echo "MODE_APPLY=$MODE normalized=$NORM DATE=$(date)"
    case "$NORM" in
        ANDROID)
            # v12: reuse a healthy Android runtime instead of destroying it.
            # v11 tore the AOA daemon down on every mode entry (and on
            # MIDI/USB_OUT bounces), which dropped the phone accessory
            # session; the recreated daemon left /tmp/r36sx_aoa_bulk_pcm_fifo
            # missing until the phone reconnected, so recording failed with
            # "runtime is not ready" and the core logged "fifo open pending".
            # v12.1: the PCM fifo is no longer required for reuse - the
            # supervisor pre-creates it, and a missing fifo is normal while
            # the phone accessory is still (re)connecting.
            reuse=0
            sup="$(cat "$RUNTIME/h35_android_supervisor_pid" 2>/dev/null || true)"
            dp="$(cat "$RUNTIME/daemon_pid" 2>/dev/null || true)"
            if [ -n "$sup" ] && kill -0 "$sup" 2>/dev/null; then
                if [ -n "$dp" ] && kill -0 "$dp" 2>/dev/null; then
                    reuse=1
                fi
            fi
            if [ "$reuse" -eq 1 ]; then
                echo "ANDROID_RUNTIME_REUSED supervisor=$sup daemon=$dp fifo=$([ -p /tmp/r36sx_aoa_bulk_pcm_fifo ] && echo present || echo missing)"
            else
                [ -x "$BIN/otg_u241_shutdown.sh" ] && /bin/sh "$BIN/otg_u241_shutdown.sh"
                for p in r36s_aoa_bulk_audio_io_h36 r36s_aoa_bulk_receiver_h36 \
                         r36s_aoa_bulk_audio_io_h35 r36s_aoa_bulk_receiver_h35; do
                    pidof "$p" >/dev/null 2>&1 && killall "$p" 2>/dev/null || true
                done
                # v14: AOA is host-side too. After a WINDOWS bounce the musb
                # is left in device/gadget role and the phone is never seen
                # ("android waiting"). Restore host stack + role like USB_OUT.
                if ! load_host_usb_modules; then
                    echo "HOST_STACK_ABORT skipped_role_switch=1"
                else
                    switch_host_role || echo "HOST_ROLE_SWITCH_FAILED rc=$?"
                    if [ -x "$BIN/otg_h37_android_runtime_supervisor.sh" ]; then
                        /bin/sh "$BIN/otg_h37_android_runtime_supervisor.sh"
                    else
                        echo "ERROR_ANDROID_SUPERVISOR_MISSING"
                    fi
                fi
            fi
            ;;
        USB_OUT)
            [ -x "$BIN/otg_u241_shutdown.sh" ] && /bin/sh "$BIN/otg_u241_shutdown.sh"
            for p in r36s_aoa_bulk_audio_io_h36 r36s_aoa_bulk_receiver_h36 \
                     r36s_aoa_bulk_audio_io_h35 r36s_aoa_bulk_receiver_h35; do
                pidof "$p" >/dev/null 2>&1 && killall "$p" 2>/dev/null || true
            done
            if ! load_host_usb_modules; then
                echo "HOST_STACK_ABORT skipped_role_switch=1"
            else
                switch_host_role || echo "HOST_ROLE_SWITCH_FAILED rc=$?"
                if [ -x "$BIN/otg_h37_host_runtime_supervisor.sh" ]; then
                    LGPT_H38_POLICY=USB_OUT_OTG /bin/sh "$BIN/otg_h37_host_runtime_supervisor.sh"
                else
                    echo "ERROR_HOST_SUPERVISOR_MISSING"
                fi
            fi
            ;;
        MIDI)
            [ -x "$BIN/otg_u241_shutdown.sh" ] && /bin/sh "$BIN/otg_u241_shutdown.sh"
            for p in r36s_aoa_bulk_audio_io_h36 r36s_aoa_bulk_receiver_h36 \
                     r36s_aoa_bulk_audio_io_h35 r36s_aoa_bulk_receiver_h35; do
                pidof "$p" >/dev/null 2>&1 && killall "$p" 2>/dev/null || true
            done
            if ! load_host_usb_modules; then
                echo "HOST_STACK_ABORT skipped_role_switch=1"
            else
                switch_host_role || echo "HOST_ROLE_SWITCH_FAILED rc=$?"
                if [ -x "$BIN/otg_h37_host_runtime_supervisor.sh" ]; then
                    LGPT_H38_POLICY=MIDI_OTG /bin/sh "$BIN/otg_h37_host_runtime_supervisor.sh"
                else
                    echo "ERROR_HOST_SUPERVISOR_MISSING"
                fi
            fi
            ;;
        WINDOWS)
            stop_host_supervisor
            for p in r36s_aoa_bulk_audio_io_h36 r36s_aoa_bulk_receiver_h36 \
                     r36s_aoa_bulk_audio_io_h35 r36s_aoa_bulk_receiver_h35 \
                     r36s_sp404_host_audio_io r36s_midi_host_io; do
                pidof "$p" >/dev/null 2>&1 && killall "$p" 2>/dev/null || true
            done
            if [ -x "$BIN/otg_u241_setup_once.sh" ]; then
                /bin/sh "$BIN/otg_u241_setup_once.sh"
            else
                echo "ERROR_WINDOWS_SETUP_MISSING"
            fi
            ;;
        *)
            stop_host_supervisor
            for p in r36s_aoa_bulk_audio_io_h36 r36s_aoa_bulk_receiver_h36 \
                     r36s_aoa_bulk_audio_io_h35 r36s_aoa_bulk_receiver_h35; do
                pidof "$p" >/dev/null 2>&1 && killall "$p" 2>/dev/null || true
            done
            [ -x "$BIN/otg_u241_shutdown.sh" ] && /bin/sh "$BIN/otg_u241_shutdown.sh"
            ;;
            esac
        # v14.1: RO-proof SD logging. If the SD FAT is mounted read-only
        # (dirty bit from a bad unplug) the old `{ ... } >> SD.log` redirect
        # aborted the whole apply, so no mode ever changed. The block logs to
        # /tmp (always writable) and mirrors to the SD log best-effort.
        if ( : >> "$LOGROOT/H38_2_APPLY_MODE.log" ) 2>/dev/null; then
            cat /tmp/h38_2_apply_mode.log >> "$LOGROOT/H38_2_APPLY_MODE.log" 2>/dev/null || true
        else
            cat /tmp/h38_2_apply_mode.log >> /tmp/h38_2_apply_mode.sd_mirror.log 2>/dev/null || true
        fi
} > /tmp/h38_2_apply_mode.log 2>&1 &

exit 0
