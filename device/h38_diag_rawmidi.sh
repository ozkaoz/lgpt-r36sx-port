#!/bin/sh
# H38 host-stack diagnostic. Forces the ALSA host module load sequence and
# captures the exact kernel "Unknown symbol" line from dmesg into
# /mnt/sdcard/LGPT_OTG_LOGS/H38_HOST_MODULE_LOAD.err (and prints it).
# Run from the console:  sh /mnt/sdcard/lgpt/otg/bin/h38_diag_rawmidi.sh
set -u
LOGROOT=/mnt/sdcard/LGPT_OTG_LOGS
H=$LOGROOT/H38_HOST_MODULE_LOAD.err
BASE=/mnt/sdcard/lgpt/otg/modules/4.4.186-release
mkdir -p "$LOGROOT" 2>/dev/null || true
{
  echo "============================================================"
  echo "H38_DIAG start $(date)"
  echo "uname: $(uname -a 2>/dev/null)"
  echo "loaded snd modules:"
  grep -E '^(snd|soundcore)' /proc/modules 2>/dev/null || echo " none"
  echo "============================================================"
} >>"$H" 2>/dev/null

load_one(){
  fn="$1"
  for p in \
    "$BASE/host_usb_audio/$fn" \
    "$BASE/u2_38au8_sync_uac2/$fn"; do
    [ -f "$p" ] || continue
    if insmod "$p" 2>>"$H"; then
      echo "OK   $fn FROM=$p" >>"$H" 2>/dev/null
      return 0
    fi
    echo "FAIL $fn FROM=$p" >>"$H" 2>/dev/null
  done
  echo "MISS $fn" >>"$H" 2>/dev/null
  return 1
}

for fn in snd-seq-device.ko snd-rawmidi.ko snd-usbmidi-lib.ko snd-usb-audio.ko; do
  load_one "$fn"
done

{
  echo "----- dmesg tail (snd|unknown|module) after load attempt -----"
  dmesg 2>/dev/null | tail -n 200 | grep -iE 'snd|unknown symbol|module|rawmidi|usbmidi|usb-audio' || echo "(dmesg unavailable or empty)"
  echo "----- END H38_DIAG -----"
} >>"$H" 2>/dev/null

# Also echo to console for immediate feedback
echo "=== dmesg filter ==="
dmesg 2>/dev/null | tail -n 200 | grep -iE 'snd|unknown symbol|rawmidi|usbmidi|usb-audio' || echo "dmesg grep empty"
echo "=== full log written to $H ==="