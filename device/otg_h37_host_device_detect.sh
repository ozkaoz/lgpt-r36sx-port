#!/bin/sh
# H38.3 host-side device detection for the unified audio driver.
# Runs after the musb OTG controller is switched to host role. It probes:
#   - SP404MKII / class-compliant UAC2 audio interface -> sp404_card=<N>
#   - USB-MIDI piano/controller -> midi_rawmidi=C<N>D0
#   - Android AOA accessory (kept for the AOA runtime)
# Writes the runtime markers consumed by the core bridge contract and the
# host runtime supervisor.
set -u
ROOT="${LGPT_SD_ROOT:-/mnt/sdcard}"
BASE="$ROOT/lgpt/otg"
RUNTIME="${LGPT_RUNTIME_DIR:-/tmp/r36sx_lgpt_usb}"
LOGROOT="${LGPT_LOGROOT:-/tmp/r36sx_lgpt_logs}"
LOG="$LOGROOT/H38_HOST_DEVICE_DETECT.log"
LOCK="${LGPT_H38_DETECT_LOCK:-/tmp/r36sx_h38_host_detect.lock}"
mkdir -p "$RUNTIME" "$LOGROOT" 2>/dev/null || true
log(){ printf '%s H38_DETECT %s\n' "$(date 2>/dev/null || echo no-date)" "$*" >>"$LOG" 2>/dev/null || true; }
atomic_write(){ p="$1"; v="$2"; d="$(dirname "$p")"; mkdir -p "$d" 2>/dev/null || true; t="${p}.h38tmp.$$"; rm -f "$t" 2>/dev/null || true; printf '%s\n' "$v" >"$t" 2>/dev/null && mv -f "$t" "$p" 2>/dev/null; }

card_dir_exists(){ [ -d "$1" ] && [ -d "$1/audio0" ]; }

detect_host(){
  # ---- SP404 / UAC2 audio interface -------------------------------------
  sp404_card="none"
  for c in /proc/asound/card*; do
    [ -d "$c" ] || continue
    idx="${c##*/card}"
    case "$idx" in ''|*[!0-9]*) continue;; esac
    [ -e "/dev/snd/pcmC${idx}D0c" ] || continue
    # Skip the built-in console card (C0 is always the R36S internal codec).
    [ "$idx" = "0" ] && continue
    if [ -d "/proc/asound/card$idx/stream0" ] || [ -d "/proc/asound/card$idx/usb" ]; then
      if [ -r "/proc/asound/card$idx/stream0" ] && grep -qi "Audio" "/proc/asound/card$idx/stream0" 2>/dev/null; then
        sp404_card="$idx"
        break
      fi
      if ls "/sys/class/sound/card$idx/device" 2>/dev/null | grep -qi usb; then
        sp404_card="$idx"
        break
      fi
    fi
  done
  atomic_write "$RUNTIME/sp404_card" "$sp404_card" || true
  log "SP404_CARD=$sp404_card"

  # ---- USB-MIDI device ---------------------------------------------------
  midi_node="none"
  for m in /dev/snd/midiC*D*; do
    [ -e "$m" ] || continue
    midi_node="${m##*/}"
    break
  done
  atomic_write "$RUNTIME/midi_rawmidi" "$midi_node" || true
  log "MIDI_RAWMIDI=$midi_node"
}

if ! mkdir "$LOCK" 2>/dev/null; then log "DETECT_BUSY"; exit 40; fi
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT INT TERM
detect_host
log "HOST_DETECT_DONE"
exit 0
