#!/bin/sh
# H38.5 host-side device detection for the unified audio driver.
# Runs after the musb OTG controller is switched to host role. It probes:
#   - SP404MKII / class-compliant UAC2 audio interface -> sp404_card=<N>
#   - USB-MIDI piano/controller -> midi_rawmidi=C<N>D<M>
#   - Android AOA accessory (kept for the AOA runtime)
# It resolves the device from the USB parent sysfs path (not by card number),
# requires BOTH playback and capture PCM nodes, discovers the real PCM device
# index D<N> and writes full PCM paths so the host backend never guesses.
# Writes the runtime markers consumed by the core bridge contract and the
# host runtime supervisor:
#   sp404_card           <N>                 (card index, for the daemon ABI)
#   sp404_playback_pcm   /dev/snd/pcmC2D0p   (full playback node)
#   sp404_capture_pcm    /dev/snd/pcmC2D0c   (full capture node)
#   sp404_stream_caps    /proc/asound/card2/stream0 (caps file)
#   sp404_usb_id         vvvv:pppp           (USB vendor/product)
#   midi_rawmidi         C1D0                (rawmidi suffix, for the daemon)
set -u
ROOT="${LGPT_SD_ROOT:-/mnt/sdcard}"
BASE="$ROOT/lgpt/otg"
RUNTIME="${LGPT_RUNTIME_DIR:-/tmp/r36sx_lgpt_usb}"
LOGROOT="${LGPT_LOGROOT:-/mnt/sdcard/LGPT_OTG_LOGS}"
LOG="$LOGROOT/H38_HOST_DEVICE_DETECT.log"
LOCK="${LGPT_H38_DETECT_LOCK:-/tmp/r36sx_h38_host_detect.lock}"
mkdir -p "$RUNTIME" "$LOGROOT" 2>/dev/null || true
log(){ printf '%s H38_DETECT %s\n' "$(date 2>/dev/null || echo no-date)" "$*" >>"$LOG" 2>/dev/null || true; }
atomic_write(){ p="$1"; v="$2"; d="$(dirname "$p")"; mkdir -p "$d" 2>/dev/null || true; t="${p}.h38tmp.$$"; rm -f "$t" 2>/dev/null || true; printf '%s\n' "$v" >"$t" 2>/dev/null && mv -f "$t" "$p" 2>/dev/null; }

# ---- helpers -----------------------------------------------------------
card_index_of(){ p="$1"; echo "${p##*/card}" ; }
is_audio_card(){ c="$1"; [ -e "/proc/asound/$c/pcm0c" ] || [ -e "/proc/asound/$c/pcm0p" ]; }
sysfs_usb_id(){ d="$1"; v="$(cat "$d/idVendor" 2>/dev/null || true)"; i="$(cat "$d/idProduct" 2>/dev/null || true)"; [ -n "$v" ] && [ -n "$i" ] && echo "$v:$i"; }
usb_tree_dump(){
  log "--- USB BUS DUMP ---"
  log "MUSB_MODE="$(find /sys/devices -path '*musb-hdrc.0.auto/mode' -exec cat {} \; 2>/dev/null | head -1 || true)
  for d in /sys/bus/usb/devices/*; do
    [ -d "$d" ] || continue
    b="$(basename "$d")"
    [ -r "$d/idVendor" ] && [ -r "$d/idProduct" ] || continue
    v="$(cat "$d/idVendor" 2>/dev/null || true)"
    p="$(cat "$d/idProduct" 2>/dev/null || true)"
    sp="$(cat "$d/speed" 2>/dev/null || true)"
    pr="$(cat "$d/product" 2>/dev/null || true)"
    mk="$(cat "$d/manufacturer" 2>/dev/null || true)"
    log "USB $b vid=$v pid=$p speed=$sp product=$pr manufacturer=$mk"
  done
  log "ASOUND_CARDS:"
  cat /proc/asound/cards 2>/dev/null >>"$LOG" || true
  log "SND_NODES:"
  ls /dev/snd 2>/dev/null >>"$LOG" || true
  log "DMESG_USB:"
  dmesg 2>/dev/null | grep -iE 'musb|usb [0-9]|otg|new high|new full|vbus|config 1|audio' | tail -n 40 >>"$LOG" || true
  log "--- USB BUS DUMP END ---"
}
sysfs_usb_path_of_card(){
  # Resolve /sys/class/sound/cardN/device -> real USB device node, then find
  # the bus address "1-1.2" that identifies the physical port.
  devlink="$1"
  parent_bus=""
  target="$(readlink -f "$devlink" 2>/dev/null || true)"
  [ -n "$target" ] || return 1
  # Walk up ancestors to the deepest node that still looks like a USB device
  # (has idVendor) and is NOT a hub root complex.
  cur="$target"
  while [ -n "$cur" ]; do
    if [ -r "$cur/idVendor" ] && [ -r "$cur/idProduct" ]; then
      parent_bus="$(echo "$cur" | sed -n 's#.*/devices/\(.*\)#\1#p')"
      case "$parent_bus" in
        *:*) break ;;          # deepest USB device found (bus path form x-y[.z])
      esac
    fi
    cur="$(dirname "$cur" 2>/dev/null || true)"
    [ "$cur" = "/" ] && break
  done
  [ -n "$parent_bus" ] && echo "$parent_bus" && return 0
  return 1
}
find_pcm_devices(){
  # Given a card dir /proc/asound/cardN, echo "D<num>" for every PCM device
  # subdir (pcmC<N>D<M>p / pcmC<N>D<M>c) that has a matching /dev/snd node.
  card="$1"; idx="$2"
  for node in /proc/asound/"$card"/pcmC*D*; do
    [ -e "$node" ] || continue
    base="$(basename "$node")"
    # base is pcmC<N>D<M> ; the actual /dev node is /dev/snd/pcmC<N>D<M>p|c
    devname="$(echo "$base" | sed -n 's/^pcm\(C[0-9]*D[0-9]*\).*/\1/p')"
    [ -n "$devname" ] || continue
    echo "$devname"
  done | sort -u
}

detect_host(){
  # ---- SP404 / UAC2 audio interface -------------------------------------
  sp404_card="none"
  sp404_pcm="none"
  sp404_play="none"
  sp404_cap="none"
  sp404_caps="none"
  sp404_usbid="none"
  sp404_syspath="none"
  sp404_found=""
  for c in /proc/asound/card*; do
    [ -d "$c" ] || continue
    idx="${c##*/card}"
    case "$idx" in ''|*[!0-9]*) continue;; esac
    # In host role the SP404MKII registers as card C0 (the R36S console uses the
    # proprietary SF3000 audio, NOT an ALSA card), so after troubleshooting the
    # UUIDs must NOT skip index 0. Every rejection is logged for diagnosis.
    if ! ls "$c"/pcmC*D* >/dev/null 2>&1; then
      log "SP404_CARD_SKIP idx=$idx reason=no_pcm_dir"
      continue
    fi
    syslink="/sys/class/sound/card$idx/device"
    if [ ! -e "$syslink" ]; then
      log "SP404_CARD_SKIP idx=$idx reason=no_syslink path=$syslink"
      continue
    fi
    rl="$(readlink -f "$syslink" 2>/dev/null || true)"
    if ! printf '%s' "$rl" | grep -qE '/usb|usb[0-9]+/'; then
      log "SP404_CARD_SKIP idx=$idx reason=not_usb_path readlink=$rl"
      continue
    fi
    # Resolve the USB parent for identity and reject anything that is not a
    # class-compliant audio interface (snd-usb-audio exposes usb_id in card dir).
    syspath="$(sysfs_usb_path_of_card "$syslink")"
    usbid="$(sysfs_usb_id "$(readlink -f "$syslink" 2>/dev/null || true)")"
    # stream0/streamN caps are regular (readable) files, not directories.
    caps="none"
    for s in "$c"/stream*; do
      [ -r "$s" ] && { caps="$s"; break; }
    done
    # Require both directions across ANY PCM device of the card, not only D0.
    found_play=""; found_cap=""; dev_found=""
    for dev in "$c"/pcmC*D*p; do
      [ -e "$dev" ] || continue
      dnum="$(echo "$dev" | sed -n 's/.*pcmC[0-9]*D\([0-9]*\)p.*/\1/p')"
      [ -n "$dnum" ] || continue
      if [ -e "/dev/snd/pcmC${idx}D${dnum}p" ]; then
        if [ -z "$found_play" ]; then found_play="$dnum"; dev_found="$dev"; fi
      fi
    done
    for dev in "$c"/pcmC*D*c; do
      [ -e "$dev" ] || continue
      dnum="$(echo "$dev" | sed -n 's/.*pcmC[0-9]*D\([0-9]*\)c.*/\1/p')"
      [ -n "$dnum" ] || continue
      if [ -e "/dev/snd/pcmC${idx}D${dnum}c" ]; then
        if [ -z "$found_cap" ]; then found_cap="$dnum"; fi
      fi
    done
    # Same D number must have both p and c (class-compliant duplex UAC2).
    [ -n "$found_play" ] && [ -n "$found_cap" ] && [ "$found_play" = "$found_cap" ] || continue
    # Prefer a real playback+capture device index that /dev exposes.
    pdev="$(basename "$dev_found" | sed -n 's/^pcm\(C[0-9]*D[0-9]*\)p.*/\1/p')"
    pcm="/dev/snd/pcm${pdev}p"
    sp404_card="$idx"
    sp404_found="card$idx"
    sp404_pcm="$pdev"
    sp404_play="/dev/snd/pcm${pdev}p"
    sp404_cap="/dev/snd/pcm${pdev}c"
    sp404_caps="$caps"
    sp404_usbid="$usbid"
    sp404_syspath="$syspath"
    log "SP404_FOUND card=$idx pcm=$pdev usb=$usbid syspath=$syspath caps=$caps"
    break
  done

  # ---- Fallback ----------------------------------------------------------
  # If no USB-backed card matched the strict path filter (the musb sysfs link
  # layout can differ), adopt the single PCM card that exposes a matched PLAY
  # + CAPTURE pair on the same D index. In host role the only ALSA audio card
  # is the connected USB device, so a sole duplex card IS the SP404.
  if [ "$sp404_card" = "none" ]; then
    for p in /dev/snd/pcmC*D*p; do
      [ -e "$p" ] || continue
      dev="${p##*/}"
      if [ -e "/dev/snd/${dev%p}c" ]; then
        idx="$(echo "$dev" | sed -n 's/^pcmC\([0-9]*\)D.*/\1/p')"
        pdev="$(echo "$dev" | sed -n 's/^pcm\(C[0-9]*D[0-9]*\)p/\1/p')"
        [ -n "$idx" ] && [ -n "$pdev" ] || continue
        sp404_card="$idx"
        sp404_pcm="$pdev"
        sp404_play="/dev/snd/${dev%p}p"
        sp404_cap="/dev/snd/${dev%p}c"
        sp404_usbid="auto:$pdev"
        sp404_syspath="$dev"
        sp404_caps="none"
        log "SP404_FALLBACK_FOUND dev=$dev idx=$idx play=$sp404_play cap=$sp404_cap"
        break
      fi
    done
  fi
  [ "$sp404_card" = "none" ] && log "SP404_NONE card_not_matched"

  # ---- Persist markers ------------------------------------------------
  atomic_write "$RUNTIME/sp404_card" "$sp404_card" || true
  atomic_write "$RUNTIME/sp404_playback_pcm" "$sp404_play" || true
  atomic_write "$RUNTIME/sp404_capture_pcm" "$sp404_cap" || true
  atomic_write "$RUNTIME/sp404_stream_caps" "$sp404_caps" || true
  atomic_write "$RUNTIME/sp404_usb_id" "$sp404_usbid" || true
  atomic_write "$RUNTIME/sp404_syspath" "$sp404_syspath" || true
  log "SP404_CARD=$sp404_card PCM=$sp404_pcm PLAY=$sp404_play CAP=$sp404_cap USBID=$sp404_usbid"

  # ---- USB-MIDI device ---------------------------------------------------
  midi_node="none"
  midi_dev="none"
  for m in /dev/snd/midiC*D*; do
    [ -e "$m" ] || continue
    midi_dev="${m##*/}"
    # In host role the only rawmidi source is the USB-MIDI device. The
    # SP-404MKII exposes midiC0D0, so do NOT skip card index 0 here.
    midi_node="$midi_dev"
    break
  done
  atomic_write "$RUNTIME/midi_rawmidi" "$midi_node" || true
  log "MIDI_RAWMIDI=$midi_node"
}

if ! mkdir "$LOCK" 2>/dev/null; then log "DETECT_BUSY"; exit 40; fi
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT INT TERM
if [ ! -e /tmp/r36sx_h38_usb_diag_done ]; then
  usb_tree_dump
  touch /tmp/r36sx_h38_usb_diag_done
fi
detect_host
log "HOST_DETECT_DONE"
exit 0
