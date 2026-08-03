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
            [ -x "$BIN/otg_u241_shutdown.sh" ] && /bin/sh "$BIN/otg_u241_shutdown.sh"
            for p in r36s_aoa_bulk_audio_io_h36 r36s_aoa_bulk_receiver_h36 \
                     r36s_aoa_bulk_audio_io_h35 r36s_aoa_bulk_receiver_h35; do
                pidof "$p" >/dev/null 2>&1 && killall "$p" 2>/dev/null || true
            done
            if [ -x "$BIN/otg_h37_android_runtime_supervisor.sh" ]; then
                /bin/sh "$BIN/otg_h37_android_runtime_supervisor.sh"
            else
                echo "ERROR_ANDROID_SUPERVISOR_MISSING"
            fi
            ;;
        USB_OUT)
            [ -x "$BIN/otg_u241_shutdown.sh" ] && /bin/sh "$BIN/otg_u241_shutdown.sh"
            for p in r36s_aoa_bulk_audio_io_h36 r36s_aoa_bulk_receiver_h36 \
                     r36s_aoa_bulk_audio_io_h35 r36s_aoa_bulk_receiver_h35; do
                pidof "$p" >/dev/null 2>&1 && killall "$p" 2>/dev/null || true
            done
            if [ -x "$BIN/otg_h37_host_runtime_supervisor.sh" ]; then
                LGPT_H38_POLICY=USB_OUT_OTG /bin/sh "$BIN/otg_h37_host_runtime_supervisor.sh"
            else
                echo "ERROR_HOST_SUPERVISOR_MISSING"
            fi
            ;;
        MIDI)
            [ -x "$BIN/otg_u241_shutdown.sh" ] && /bin/sh "$BIN/otg_u241_shutdown.sh"
            for p in r36s_aoa_bulk_audio_io_h36 r36s_aoa_bulk_receiver_h36 \
                     r36s_aoa_bulk_audio_io_h35 r36s_aoa_bulk_receiver_h35; do
                pidof "$p" >/dev/null 2>&1 && killall "$p" 2>/dev/null || true
            done
            if [ -x "$BIN/otg_h37_host_runtime_supervisor.sh" ]; then
                LGPT_H38_POLICY=MIDI_OTG /bin/sh "$BIN/otg_h37_host_runtime_supervisor.sh"
            else
                echo "ERROR_HOST_SUPERVISOR_MISSING"
            fi
            ;;
        WINDOWS)
            for p in r36s_aoa_bulk_audio_io_h36 r36s_aoa_bulk_receiver_h36 \
                     r36s_aoa_bulk_audio_io_h35 r36s_aoa_bulk_receiver_h35; do
                pidof "$p" >/dev/null 2>&1 && killall "$p" 2>/dev/null || true
            done
            if [ -x "$BIN/otg_u241_setup_once.sh" ]; then
                /bin/sh "$BIN/otg_u241_setup_once.sh"
            else
                echo "ERROR_WINDOWS_SETUP_MISSING"
            fi
            ;;
        *)
            for p in r36s_aoa_bulk_audio_io_h36 r36s_aoa_bulk_receiver_h36 \
                     r36s_aoa_bulk_audio_io_h35 r36s_aoa_bulk_receiver_h35; do
                pidof "$p" >/dev/null 2>&1 && killall "$p" 2>/dev/null || true
            done
            [ -x "$BIN/otg_u241_shutdown.sh" ] && /bin/sh "$BIN/otg_u241_shutdown.sh"
            ;;
    esac
} >> "$LOGROOT/H38_2_APPLY_MODE.log" 2>&1 &

exit 0
