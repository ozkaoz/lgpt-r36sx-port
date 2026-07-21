#!/bin/sh
ROOT=/mnt/sdcard
PICO="$ROOT/cubegm/picoarch"
CORE="$ROOT/cubegm/cores/lgpt_r36sx_port_libretro.so"
ROM="${1:-$ROOT/roms/lgpt/start.lgpt}"
DATA="$ROOT/lgpt"
LOGROOT="$ROOT/LGPT_OTG_LOGS"
LOG="$LOGROOT/LGPT_U241_LAUNCHER.log"

mkdir -p "$DATA" "$DATA/samples" "$DATA/project" "$DATA/projects" \
         "$DATA/usbrecs" "$DATA/otg/logs" "$LOGROOT" 2>/dev/null || true

{
  echo "=== LGPT U2.52.3 ABI7 rename-caret alignment github-final launch $(date) ==="
  echo "PICO=$PICO"
  echo "CORE=$CORE"
  echo "ROM=$ROM"
  file "$CORE" 2>/dev/null || true
} >>"$LOG" 2>&1

# OTG setup is nonblocking; a USB failure never blocks LOCAL_CONSOLE.
if [ -x "$DATA/otg/bin/otg_u241_setup_once.sh" ]; then
  "$DATA/otg/bin/otg_u241_setup_once.sh" >>"$LOG" 2>&1 &
fi

cd "$DATA" || exit 2
exec "$PICO" "$CORE" "$ROM" >>"$LOG" 2>&1
