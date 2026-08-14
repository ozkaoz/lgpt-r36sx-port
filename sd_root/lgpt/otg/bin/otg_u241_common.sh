#!/bin/sh
BASE=/mnt/sdcard/lgpt/otg
MODBASE=$BASE/modules/4.4.186-release
AU8DIR=$MODBASE/u2_38au8_sync_uac2
AU8MODULE=$AU8DIR/usb_f_uac2.ko
LOGROOT=/mnt/sdcard/LGPT_OTG_LOGS
INTERNAL_LOG=$BASE/logs
RUNTIME=/tmp/r36sx_lgpt_usb
GADGET=/sys/kernel/config/usb_gadget/r36sx_lgpt_u2414
UDC_NAME=musb-hdrc.0.auto

mkdir -p "$LOGROOT" "$INTERNAL_LOG" "$RUNTIME" "$AU8DIR" 2>/dev/null || true

u2414_loaded() {
    grep -q "^$1 " /proc/modules 2>/dev/null
}

u2414_try_modprobe() {
    name="$1"
    command -v modprobe >/dev/null 2>&1 || return 1
    modprobe "$name" 2>/tmp/u2414_modprobe.err
    rc=$?
    echo "MODPROBE_${name}_RC=$rc ERR=$(cat /tmp/u2414_modprobe.err 2>/dev/null)"
    return "$rc"
}

u2414_load_module_file() {
    filename="$1"
    module="$(echo "${filename%.ko}" | tr '-' '_')"

    if u2414_loaded "$module"; then
        echo "LOAD_${filename}_ALREADY=YES"
        return 0
    fi

    u2414_try_modprobe "$module" && return 0

    for p in \
      /lib/modules/4.4.186-release/kernel/sound/core/"$filename" \
      /lib32/modules/4.4.186-release/kernel/sound/core/"$filename" \
      /lib/modules/4.4.186-release/kernel/drivers/usb/gadget/"$filename" \
      /lib32/modules/4.4.186-release/kernel/drivers/usb/gadget/"$filename" \
      $(find "$MODBASE" -type f -name "$filename" 2>/dev/null); do
        [ -f "$p" ] || continue
        insmod "$p" 2>/tmp/u2414_insmod.err
        rc=$?
        echo "LOAD_${filename}_RC=$rc FROM=$p ERR=$(cat /tmp/u2414_insmod.err 2>/dev/null)"
        [ "$rc" -eq 0 ] && return 0
    done

    echo "LOAD_${filename}_MISSING_OR_FAILED=YES"
    return 2
}

u2414_remove_gadget() {
    G="$1"
    [ -d "$G" ] || return 0

    echo "" > "$G/UDC" 2>/dev/null || true
    sleep 1

    find "$G/configs" -type l -exec rm -f {} \; 2>/dev/null || true
    find "$G/functions" -mindepth 1 -maxdepth 1 -type d \
        -exec rmdir {} \; 2>/dev/null || true
    find "$G/configs" -depth -type d -exec rmdir {} \; 2>/dev/null || true
    find "$G/strings" -depth -type d -exec rmdir {} \; 2>/dev/null || true
    rmdir "$G" 2>/dev/null || true
}

u2414_cleanup_gadgets() {
    mount -t configfs none /sys/kernel/config 2>/dev/null || true

    for G in /sys/kernel/config/usb_gadget/r36sx_lgpt_* \
             /sys/kernel/config/usb_gadget/r36sx_uac2_*; do
        [ -d "$G" ] || continue
        echo "CLEAN_GADGET=$G"
        u2414_remove_gadget "$G"
    done
}

u2414_unload_uac2() {
    if ! u2414_loaded usb_f_uac2; then
        echo "UNLOAD_usb_f_uac2_NOT_LOADED=YES"
        return 0
    fi

    if command -v rmmod >/dev/null 2>&1; then
        rmmod usb_f_uac2 2>/tmp/u2414_rmmod.err
        rc=$?
    elif command -v modprobe >/dev/null 2>&1; then
        modprobe -r usb_f_uac2 2>/tmp/u2414_rmmod.err
        rc=$?
    else
        echo "ERROR_NO_RMMOD_OR_MODPROBE=YES"
        return 2
    fi

    echo "UNLOAD_usb_f_uac2_RC=$rc ERR=$(cat /tmp/u2414_rmmod.err 2>/dev/null)"
    if [ "$rc" -eq 0 ]; then
        if u2414_loaded usb_f_uac2; then
            echo "ERROR_usb_f_uac2_STILL_LOADED=YES"
            return 3
        fi
        return 0
    fi

    # U2.53 CONFIG_MODULE_UNLOAD=n kernels: rmmod/modprobe -r fail with
    # "Function not implemented" (EOPNOTSUPP) and the module stays loaded.
    # u2414_cleanup_gadgets already destroyed every gadget that referenced
    # it, so a still-loaded module is reusable; u2414_load_stack tolerates
    # the resulting "File exists" from insmod. Without this, every runtime
    # rebuild exited 4 and the Windows gadget was never created.
    if u2414_loaded usb_f_uac2 && \
       grep -qi 'not implemented\|not supported' /tmp/u2414_rmmod.err 2>/dev/null; then
        echo "UNLOAD_usb_f_uac2_NOT_SUPPORTED=YES (module unload disabled in kernel; module stays loaded)"
        return 0
    fi
    return "$rc"
}

u2414_load_stack() {
    rc=0
    for m in soundcore.ko snd.ko snd-timer.ko snd-pcm.ko; do
        u2414_load_module_file "$m" || rc=2
    done

    u2414_loaded libcomposite ||
        u2414_load_module_file libcomposite.ko || rc=2

    [ -s "$AU8MODULE" ] || {
        echo "ERROR_AU8_SYNC_MODULE_MISSING=$AU8MODULE"
        return 4
    }

    insmod "$AU8MODULE" 2>/tmp/u2414_uac2_insmod.err
    load_rc=$?
    echo "LOAD_AU8_SYNC_UAC2_RC=$load_rc FROM=$AU8MODULE ERR=$(cat /tmp/u2414_uac2_insmod.err 2>/dev/null)"
    if [ "$load_rc" -ne 0 ]; then
        # U2.53 CONFIG_MODULE_UNLOAD=n: after a tolerated "unload", the
        # module is already loaded and insmod reports "File exists". The
        # loaded module is usable, so keep going.
        if u2414_loaded usb_f_uac2 && \
           grep -qi 'file exists\|already' /tmp/u2414_uac2_insmod.err 2>/dev/null; then
            echo "LOAD_AU8_SYNC_UAC2_ALREADY_LOADED=YES"
        else
            return "$load_rc"
        fi
    fi

    u2414_loaded usb_f_uac2 || {
        echo "ERROR_AU8_SYNC_UAC2_NOT_LOADED=YES"
        return 5
    }

    return "$rc"
}

u2414_switch_peripheral() {
    p=/sys/devices/platform/soc/18844000.usb/musb-hdrc.0.auto/mode
    if [ ! -e "$p" ]; then
        p="$(find /sys/devices -path '*musb-hdrc.0.auto/mode' \
            -print -quit 2>/dev/null)"
    fi

    [ -n "$p" ] && [ -e "$p" ] || {
        echo "ERROR_USB0_ROLE_PATH_MISSING=YES"
        return 2
    }

    echo "ROLE_BEFORE $p=$(cat "$p" 2>/dev/null)"
    echo peripheral > "$p" 2>/tmp/u2414_role.err
    rc=$?
    echo "ROLE_WRITE_RC=$rc PATH=$p ERR=$(cat /tmp/u2414_role.err 2>/dev/null)"
    echo "ROLE_AFTER $p=$(cat "$p" 2>/dev/null)"
    return "$rc"
}

u2414_create_gadget() {
    mount -t configfs none /sys/kernel/config 2>/dev/null || true
    [ -d /sys/kernel/config/usb_gadget ] || {
        echo "ERROR_CONFIGFS_USB_GADGET_ABSENT=YES"
        return 2
    }

    u2414_switch_peripheral || return $?

    mkdir -p "$GADGET" || return 2
    cd "$GADGET" || return 2

    echo 0x1209 > idVendor
    echo 0x38E8 > idProduct
    # U2.51.7 preserves the validated Windows USB identity intentionally.
    echo 0x2516 > bcdDevice
    echo 0x0200 > bcdUSB

    # Interface-defined class; UAC2 interfaces advertise class 01/protocol 20.
    echo 0x00 > bDeviceClass
    echo 0x00 > bDeviceSubClass
    echo 0x00 > bDeviceProtocol

    mkdir -p strings/0x409 configs/c.1/strings/0x409
    echo "R36SX-U2516-48K" > strings/0x409/serialnumber
    echo "R36SX" > strings/0x409/manufacturer
    echo "R36SX USB AUDIO 48K" > strings/0x409/product
    echo "R36SX USB AUDIO 48K AU8 SYNC" \
        > configs/c.1/strings/0x409/configuration
    echo 120 > configs/c.1/MaxPower

    mkdir -p functions/uac2.usb0 2>/tmp/u2414_uac2_create.err || {
        echo "ERROR_CREATE_UAC2_FUNCTION=YES ERR=$(cat /tmp/u2414_uac2_create.err 2>/dev/null)"
        return 3
    }

    # U2.51.4 quality profile.
    #
    # 48 kHz is retained on USB because it produces an integer 48-frame
    # isochronous packet cadence at 1 ms.  44.1 kHz would require alternating
    # fractional packet sizes and is less robust on this synchronous endpoint.
    #
    # U2.51.4 retains MONO_48K as the validated compatibility baseline.
    # STEREO_48K remains available as an explicit second-stage experiment.
    # This isolates descriptor/channel regressions before re-enabling stereo.
    PROFILE_FILE=/mnt/sdcard/lgpt/otg/audio_usb_profile
    AUDIO_PROFILE="$(cat "$PROFILE_FILE" 2>/dev/null || true)"
    [ -n "$AUDIO_PROFILE" ] || AUDIO_PROFILE=MONO_48K

    case "$AUDIO_PROFILE" in
        STEREO_48K)
            AUDIO_CHMASK=3
            AUDIO_CHANNELS=2
            ;;
        *)
            AUDIO_PROFILE=MONO_48K
            AUDIO_CHMASK=1
            AUDIO_CHANNELS=1
            ;;
    esac

    echo "$AUDIO_CHMASK" > functions/uac2.usb0/p_chmask
    echo 48000 > functions/uac2.usb0/p_srate
    echo 2 > functions/uac2.usb0/p_ssize
    echo "$AUDIO_CHMASK" > functions/uac2.usb0/c_chmask
    echo 48000 > functions/uac2.usb0/c_srate
    echo 2 > functions/uac2.usb0/c_ssize

    mkdir -p /tmp/r36sx_lgpt_usb 2>/dev/null || true
    printf '%s\n' "$AUDIO_PROFILE"         > /tmp/r36sx_lgpt_usb/audio_profile
    printf '%s\n' "$AUDIO_CHANNELS"         > /tmp/r36sx_lgpt_usb/audio_channels
    printf '48000\n'         > /tmp/r36sx_lgpt_usb/audio_rate

    ln -s functions/uac2.usb0 configs/c.1/uac2.usb0 || return 4

    echo "DESCRIPTOR_U2517_READY=YES"
    echo "PRODUCT=R36SX USB AUDIO 48K"
    echo "ENDPOINT_SYNC=SYNCHRONOUS_IN_AND_OUT"
    echo "PROFILE=${AUDIO_PROFILE}_16BIT_DUPLEX"
    echo "CHANNELS=$AUDIO_CHANNELS"
    return 0
}

u2414_bind() {
    [ -e "/sys/class/udc/$UDC_NAME" ] || {
        echo "ERROR_REQUIRED_UDC_MISSING=$UDC_NAME"
        return 2
    }

    echo "$UDC_NAME" > "$GADGET/UDC" 2>/tmp/u2414_bind.err
    rc=$?
    echo "BIND_RC=$rc UDC=$UDC_NAME ERR=$(cat /tmp/u2414_bind.err 2>/dev/null)"
    [ "$rc" -eq 0 ] || return "$rc"

    [ "$(cat "$GADGET/UDC" 2>/dev/null)" = "$UDC_NAME" ] || {
        echo "ERROR_UDC_NOT_BOUND=YES"
        return 3
    }
    return 0
}

u2414_wait_alsa() {
    n=0
    while [ "$n" -lt 15 ]; do
        if [ -e /dev/snd/pcmC0D0p ] &&
           [ -e /dev/snd/pcmC0D0c ]; then
            echo "ALSA_PCM_READY_AFTER=${n}s"
            return 0
        fi
        sleep 1
        n=$((n + 1))
    done

    echo "ERROR_ALSA_PCM_TIMEOUT=YES"
    return 2
}

u2414_snapshot() {
    OUT="$LOGROOT/U2517_AUDIO_DRIVER_SNAPSHOT.txt"
    {
        date
        uname -a
        echo "--- module ---"
        file "$AU8MODULE" 2>/dev/null || true
        sha256sum "$AU8MODULE" 2>/dev/null || true
        modinfo "$AU8MODULE" 2>/dev/null || true
        strings "$AU8MODULE" 2>/dev/null |
            grep -E 'R36SX_U2414|R36SX USB AUDIO' || true
        echo "--- modules loaded ---"
        cat /proc/modules 2>/dev/null |
            grep -E '^(soundcore|snd|libcomposite|usb_f_uac2)' || true
        echo "--- UDC ---"
        for u in /sys/class/udc/*; do
            [ -e "$u/state" ] || continue
            echo "UDC=$(basename "$u") STATE=$(cat "$u/state" 2>/dev/null) SPEED=$(cat "$u/current_speed" 2>/dev/null)"
        done
        echo "--- ALSA ---"
        ls -la /dev/snd 2>/dev/null || true
        cat /proc/asound/cards 2>/dev/null || true
        cat /proc/asound/pcm 2>/dev/null || true
        echo "--- gadget ---"
        if [ -d "$GADGET" ]; then
            for f in idVendor idProduct bcdDevice bcdUSB \
                     bDeviceClass bDeviceSubClass bDeviceProtocol UDC; do
                echo "$f=$(cat "$GADGET/$f" 2>/dev/null)"
            done
            for f in p_chmask p_srate p_ssize \
                     c_chmask c_srate c_ssize; do
                echo "$f=$(cat "$GADGET/functions/uac2.usb0/$f" 2>/dev/null)"
            done
        fi
        echo "--- daemon ---"
        pidof r36s_u241_usb_audio_io 2>/dev/null || true
        find "$RUNTIME" -maxdepth 1 -type f -exec sh -c \
            'echo "### $1"; cat "$1"' _ {} \; 2>/dev/null || true
        echo "--- dmesg ---"
        dmesg 2>/dev/null | tail -350
    } > "$OUT" 2>&1

    cp -f "$OUT" "$INTERNAL_LOG/U2517_AUDIO_DRIVER_SNAPSHOT.txt" \
        2>/dev/null || true
    echo "SNAPSHOT=$OUT"
}
