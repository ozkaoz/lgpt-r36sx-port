#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE="$ROOT/source/sources"
TMP="${TMPDIR:-/tmp}/u2523_host"
rm -rf "$TMP" && mkdir -p "$TMP"
BASE=(
  -std=gnu++03 -DPLATFORM_TREEFROG -DTREEFROG_UAC2_BRIDGE=1
  -DTREEFROG_INPUT_PROFILE=0 -DTREEFROG_INPUT_DEBUG=0
  -DTREEFROG_AUDIO_MODE=0 -DTREEFROG_TIMER_MODE=1
  -DTREEFROG_FACE_TAP_MODE=0 -DTREEFROG_MODIFIER_RETRIGGER=0
  -DTREEFROG_EVENT_DEBUG_ALL=1 -DTREEFROG_DISABLE_PHRASE_AUDITION=0
  -DTREEFROG_HIGH_CONTRAST_SELECTION=1 -DTREEFROG_PURPLE_FOCUS_SELECTION=1
  -DTREEFROG_PURPLE_FOCUS_RGB565=0xd99b -DTREEFROG_VIDEO_MODE=0
  -DTREEFROG_VIDEO_PROBE=0 -DTREEFROG_PORT_VERSION_BADGE=0
  -DTREEFROG_AUDIO_DEBUG=0 -DTREEFROG_RETRO_LIFECYCLE_DEBUG=0
  -DCPP_MEMORY -DHAVE_STDINT_H -D_NDEBUG -D_NO_JACK_ -DDUMMYMIDI
  -D_LGPT_NO_SCREEN_CACHE_ -include stdint.h
  -include "$SOURCE/Adapters/TREEFROG/Compat/SDL_types_force.h"
  -I"$SOURCE/Adapters/TREEFROG/Compat" -I"$SOURCE"
)
FILES=(
  "$SOURCE/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp"
  "$SOURCE/Adapters/TREEFROG/Main/CrashTrap.cpp"
  "$SOURCE/Adapters/TREEFROG/GUI/TreeFrogEventManager.cpp"
  "$SOURCE/Application/AppWindow.cpp"
  "$SOURCE/Application/Views/InstrumentView.cpp"
  "$SOURCE/Application/Views/ModalDialogs/UsbRecordModal.cpp"
  "$SOURCE/Application/Views/ModalDialogs/ImportSampleDialog.cpp"
  "$SOURCE/Application/Instruments/SamplePool.cpp"
  "$SOURCE/Application/Instruments/SampleVariable.cpp"
  "$SOURCE/Application/Views/ModalDialogs/SampleChopperModal.cpp"
  "$SOURCE/Application/Views/MixerView.cpp"
  "$SOURCE/Application/Audio/AudioFileStreamer.cpp"
  "$SOURCE/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp"
)
for file_path in "${FILES[@]}"; do
  g++ "${BASE[@]}" -DTREEFROG_ENABLE_START=1 -DTREEFROG_ENABLE_SELECT=1 -fsyntax-only "$file_path"
done

# Diagnostic branches must also remain compilable, but the production build uses 0.
for file_path in \
  "$SOURCE/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp" \
  "$SOURCE/Adapters/TREEFROG/GUI/TreeFrogEventManager.cpp" \
  "$SOURCE/Application/Views/ModalDialogs/UsbRecordModal.cpp"; do
  g++ "${BASE[@]}" -UTREEFROG_INPUT_DEBUG -DTREEFROG_INPUT_DEBUG=1 \
    -DTREEFROG_ENABLE_START=1 -DTREEFROG_ENABLE_SELECT=1 \
    -fsyntax-only "$file_path"
done

OBJECT="$TMP/TreeFrogLibretro.o"
g++ "${BASE[@]}" -DTREEFROG_ENABLE_START=1 -DTREEFROG_ENABLE_SELECT=1 \
  -O2 -ffunction-sections -fdata-sections -fPIC \
  -c "$SOURCE/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp" -o "$OBJECT"
readelf -Ws "$OBJECT" > "$TMP/readelf.txt"
grep -Fq 'TreeFrogU2510GlobalChopStereoBuildMarker' "$TMP/readelf.txt"
strings "$OBJECT" > "$TMP/strings.txt"
grep -Fq 'U2510_GLOBAL_CHOP_HISTORY' "$TMP/strings.txt"
grep -Fq 'U2510_GLOBAL_CHOPPER_HISTORY_24_OVERLAY_SAFE' "$TMP/strings.txt"
grep -Fq 'U2510_STEREO_48K_DYNAMIC_PROFILE' "$TMP/strings.txt"

RECORD_OBJECT="$TMP/UsbRecordModal.o"
g++ "${BASE[@]}" -DTREEFROG_ENABLE_START=1 -DTREEFROG_ENABLE_SELECT=1 \
  -O2 -ffunction-sections -fdata-sections -fPIC \
  -c "$SOURCE/Application/Views/ModalDialogs/UsbRecordModal.cpp" \
  -o "$RECORD_OBJECT"
readelf -Ws "$RECORD_OBJECT" > "$TMP/UsbRecordModal.readelf.txt"
grep -Fq 'TreeFrogU2520RecordBuildMarker' "$TMP/UsbRecordModal.readelf.txt"
strings "$RECORD_OBJECT" > "$TMP/UsbRecordModal.strings.txt"
grep -Fq 'U2520_PENDING_TAKE_RECORD_EDITOR_PHYSICAL_EDGE_READY' \
  "$TMP/UsbRecordModal.strings.txt"
echo U2520_RECORD_MODAL_MARKER_OBJECT_OK

BRIDGE_OBJECT="$TMP/TreeFrogUac2Bridge.o"
g++ "${BASE[@]}" -DTREEFROG_ENABLE_START=1 -DTREEFROG_ENABLE_SELECT=1 \
  -O2 -ffunction-sections -fdata-sections -fPIC \
  -c "$SOURCE/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp" \
  -o "$BRIDGE_OBJECT"
strings "$BRIDGE_OBJECT" > "$TMP/TreeFrogUac2Bridge.strings.txt"
for marker in U2517_MONITOR_FIFO_HANDSHAKE_CONTROL U2517_FILENAME_EDITOR_FAST_CASE_DUPLICATE_GUARD U2517_RUNTIME_ABI7_DAEMON_ONLY_RECOVERY; do
  grep -Fq "$marker" "$TMP/TreeFrogUac2Bridge.strings.txt"
done
echo U2520_INHERITED_U2517_BRIDGE_MARKERS_OBJECT_OK

CHOPPER_OBJECT="$TMP/SampleChopperModal.o"
CHOPPER_READELF="$TMP/SampleChopperModal.readelf.txt"
CHOPPER_STRINGS="$TMP/SampleChopperModal.strings.txt"

g++ "${BASE[@]}" -DTREEFROG_ENABLE_START=1 -DTREEFROG_ENABLE_SELECT=1 \
  -O2 -ffunction-sections -fdata-sections -fPIC \
  -c "$SOURCE/Application/Views/ModalDialogs/SampleChopperModal.cpp" \
  -o "$CHOPPER_OBJECT"

readelf -Ws "$CHOPPER_OBJECT" > "$CHOPPER_READELF"
grep -Fq 'LgptU2510GlobalChopperHistoryBuildMarker' "$CHOPPER_READELF"
strings "$CHOPPER_OBJECT" > "$CHOPPER_STRINGS"
grep -Fq 'U2510_GLOBAL_CHOPPER_HISTORY_24_OVERLAY_SAFE' "$CHOPPER_STRINGS"
echo U2510_EXPORTED_CHOPPER_MARKER_OBJECT_OK
gcc -std=gnu99 -Wall -Wextra -Werror=implicit-function-declaration \
  -fsyntax-only "$ROOT/device/r36s_u2523_usb_audio_io.c"
sh -n "$ROOT/device/otg_u241_common.sh"
sh -n "$ROOT/device/otg_u241_setup_once.sh"
IMPORT_OBJECT="$TMP/ImportSampleDialog.o"
g++ "${BASE[@]}" -DTREEFROG_ENABLE_START=1 -DTREEFROG_ENABLE_SELECT=1 \
  -O2 -ffunction-sections -fdata-sections -fPIC \
  -c "$SOURCE/Application/Views/ModalDialogs/ImportSampleDialog.cpp" \
  -o "$IMPORT_OBJECT"
readelf -Ws "$IMPORT_OBJECT" > "$TMP/ImportSampleDialog.readelf.txt"
grep -Fq 'TreeFrogU2523BrowserManageBuildMarker' "$TMP/ImportSampleDialog.readelf.txt"
strings "$IMPORT_OBJECT" > "$TMP/ImportSampleDialog.strings.txt"
grep -Fq 'U2523_RENAME_CARET_ALIGNMENT_GITHUB_FINAL' \
  "$TMP/ImportSampleDialog.strings.txt"
echo U2523_RENAME_CARET_ALIGNMENT_OBJECT_OK

echo HOST_SYNTAX_CHECK_U2523_OK
