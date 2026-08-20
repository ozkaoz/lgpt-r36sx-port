#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
TC="${TC:-$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot}"
INPUT_DEBUG_VALUE="${TREEFROG_INPUT_DEBUG:-0}"
EVENT_DEBUG_VALUE="${TREEFROG_EVENT_DEBUG_ALL:-0}"
AUDIO_DEBUG_VALUE="${TREEFROG_AUDIO_DEBUG:-0}"

echo "TREEFROG_INPUT_DEBUG=$INPUT_DEBUG_VALUE"
echo "TREEFROG_EVENT_DEBUG_ALL=$EVENT_DEBUG_VALUE"
echo "TREEFROG_AUDIO_DEBUG=$AUDIO_DEBUG_VALUE"

cd "$ROOT/projects"

# BACON_1.5_BLUE_FOCUS (U2.57, feedback #10): the focus/selection accent
# is BLUE (0x9dbf, RGB565 152,216,248), the code default -- it was
# 0xd99b (216,152,216, pink) here, which made every selection read
# rosado on the console ("los colores se ven rosados, deben ser
# azules-morados").  NOTE: keep comments ABOVE the make command -- a
# comment inside a backslash-continued command line breaks the chain.
rm -rf buildTREEFROG
rm -f lgpt_libretro.so ../lgpt_libretro.so
find . -type f \( -name '*.o' -o -name '*.d' -o -name '*.so' \) -delete 2>/dev/null || true

make PLATFORM=TREEFROG \
  TOOLCHAIN="$TC" \
  TREEFROG_INPUT_PROFILE=0 \
  TREEFROG_ENABLE_START=1 \
  TREEFROG_ENABLE_SELECT=1 \
  TREEFROG_AUDIO_MODE=0 \
  TREEFROG_TIMER_MODE=1 \
  TREEFROG_FACE_TAP_MODE=0 \
  TREEFROG_MODIFIER_RETRIGGER=0 \
  TREEFROG_DISABLE_PHRASE_AUDITION=0 \
  TREEFROG_HIGH_CONTRAST_SELECTION=1 \
  TREEFROG_PURPLE_FOCUS_SELECTION=1 \
  TREEFROG_PURPLE_FOCUS_RGB565=0x9dbf \
  TREEFROG_VIDEO_MODE=0 \
  TREEFROG_VIDEO_PROBE=0 \
  TREEFROG_PORT_VERSION_BADGE=0 \
  TREEFROG_PORT_VERSION_BADGE_COLOR=0x9dbf \
  TREEFROG_INPUT_DEBUG="$INPUT_DEBUG_VALUE" \
  TREEFROG_EVENT_DEBUG_ALL="$EVENT_DEBUG_VALUE" \
  TREEFROG_RETRO_LIFECYCLE_DEBUG=0 \
  TREEFROG_AUDIO_DEBUG="$AUDIO_DEBUG_VALUE" \
  -j1

mkdir -p "$ROOT/dist"
cp -f "$ROOT/projects/lgpt_libretro.so" "$ROOT/dist/lgpt_libretro.so"
ls -lh "$ROOT/dist/lgpt_libretro.so"
