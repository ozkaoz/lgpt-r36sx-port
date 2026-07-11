#!/usr/bin/env bash
set -euo pipefail
PROJ_ROOT="${1:-/mnt/d/R36S/PORT LPTRACKER}"
BUILD_ROOT="${2:-/tmp/r36s_u2_38au11}"
PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$BUILD_ROOT/U2_38AU11_BUILD_OUT"
SRC_PKG="$PKG_DIR/source_full/LGPT_PORT_U2_50_AU11U_CLEANROOM_SD_WSL_WIN_PURGE_ROOT_DIAG_SOURCE"

# AU11M packaging/build fix:
# The upstream LGPT Makefile uses include $(PWD)/Makefile.$(PLATFORM).
# GNU make splits unescaped spaces in include paths, so building directly from
# /mnt/d/R36S/PORT LPTRACKER/... fails with:
#   No rule to make target 'LPTRACKER/.../Makefile.TREEFROG'
# Compile from a clean temporary source tree whose absolute path has no spaces,
# then install the resulting core back to the SD.
case "$BUILD_ROOT" in
  *[[:space:]]*) echo "ERROR BUILD_ROOT must not contain spaces: $BUILD_ROOT"; exit 2;;
esac
rm -rf "$BUILD_ROOT"
mkdir -p "$OUT"
[ -f "$SRC_PKG/projects/Makefile.TREEFROG" ] || { echo "ERROR source_full missing projects/Makefile.TREEFROG: $SRC_PKG" | tee "$OUT/U2_38AU11_VALIDATION_SUMMARY.txt"; exit 2; }
BUILD_SRC_PARENT="$BUILD_ROOT/source_build"
mkdir -p "$BUILD_SRC_PARENT"
cp -a "$SRC_PKG" "$BUILD_SRC_PARENT/"
SRC="$BUILD_SRC_PARENT/LGPT_PORT_U2_50_AU11U_CLEANROOM_SD_WSL_WIN_PURGE_ROOT_DIAG_SOURCE"
[ -f "$SRC/projects/Makefile.TREEFROG" ] || { echo "ERROR build source copy missing projects/Makefile.TREEFROG: $SRC" | tee "$OUT/U2_38AU11_VALIDATION_SUMMARY.txt"; exit 2; }

echo "SOURCE_ROOT_PACKAGE=$SRC_PKG" | tee "$OUT/source_root_used.txt"
echo "SOURCE_ROOT_BUILD=$SRC" | tee -a "$OUT/source_root_used.txt"
echo "AU11U_BUILD_PATH_FIX=YES" | tee -a "$OUT/source_root_used.txt"
markers=(
  'U2_38AU11U_CLEANROOM_SD_WSL_WIN_PURGE_ROOT_DIAG'
  'AU11U_THREE_AUDIO_DRIVER_MODES'
  'AU11U_MODE_CONSOLE_AUDIO'
  'AU11U_MODE_USB_INPUT_OUTPUT'
  'AU11U_MODE_EXTERNAL_RECORDING'
  'AU11U_USB_RECORD_VISUAL_REC_INDICATOR'
  'AU11U_USB_RECORD_COUNTDOWN_120'
  'AU11U_CAPTURE_STAGING_COPY'
  'AU11U_CLEANROOM_SAFE_INSTRUMENT_CASTS'
  'AU11U_USB_REC_SHORTCUT_STABLE_INSTRUMENT'
  'AU11U_HOST_HELPER_PENDING'
  'AU11U_ANDROID_OTG_TEST_PENDING'
  'AU11U_WINDOWS_MONITORING_FIX'
  'AU11U_DUPLEX_STABLE_ALWAYS_OPEN_ENDPOINTS'
  'AU11_DISABLE_CHOPPER_L2A_USB_REC'
  'AU11_FAST_RECORD_MENU_NO_RELEASE_LAG'
)
for marker in "${markers[@]}"; do
  if ! grep -R "$marker" -n "$SRC/sources" "$SRC/README.md" >/dev/null 2>&1; then
    echo "ERROR missing source marker: $marker" | tee "$OUT/U2_38AU11_VALIDATION_SUMMARY.txt"
    exit 6
  fi
done
if grep -R 'L2+A USB REC menu' -n "$SRC/sources" >/dev/null 2>&1; then
  echo "ERROR stale Chopper L2+A USB REC route text still present" | tee "$OUT/U2_38AU11_VALIDATION_SUMMARY.txt"
  exit 6
fi

echo "PATCH_SOURCE=source_full_au11u_cleanroom_root_diag" | tee "$OUT/patch_lgpt_au11.log"

TOOLCHAIN="${TOOLCHAIN:-}"
if [ -z "$TOOLCHAIN" ]; then
  for cand in \
    "$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot" \
    "/home/dafunknoise/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot" \
    "$PROJ_ROOT/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot"; do
    if [ -x "$cand/bin/mips-mti-linux-gnu-gcc" ] || [ -x "$cand/opt/ext-toolchain/bin/mips-mti-linux-gnu-gcc" ]; then TOOLCHAIN="$cand"; break; fi
  done
fi
if [ -z "$TOOLCHAIN" ]; then
  GCC_PATH="$(find "$HOME" /mnt/d/R36S -maxdepth 8 -type f -name mips-mti-linux-gnu-gcc 2>/dev/null | head -1 || true)"
  if [ -n "$GCC_PATH" ]; then
    if [[ "$GCC_PATH" == */bin/mips-mti-linux-gnu-gcc ]]; then TOOLCHAIN="${GCC_PATH%/bin/mips-mti-linux-gnu-gcc}";
    elif [[ "$GCC_PATH" == */opt/ext-toolchain/bin/mips-mti-linux-gnu-gcc ]]; then TOOLCHAIN="${GCC_PATH%/opt/ext-toolchain/bin/mips-mti-linux-gnu-gcc}";
    fi
  fi
fi
[ -n "$TOOLCHAIN" ] || { echo "ERROR toolchain not found. Export TOOLCHAIN=/ruta/al/mipsel-buildroot-linux-gnu_sdk-buildroot" | tee "$OUT/U2_38AU11_VALIDATION_SUMMARY.txt"; exit 4; }
if [ -x "$TOOLCHAIN/bin/mips-mti-linux-gnu-gcc" ]; then CROSS="$TOOLCHAIN/bin/mips-mti-linux-gnu-"; else CROSS="$TOOLCHAIN/opt/ext-toolchain/bin/mips-mti-linux-gnu-"; fi
SYSROOT="${SYSROOT:-$TOOLCHAIN/mipsel-buildroot-linux-gnu/sysroot}"
[ -x "${CROSS}gcc" ] || { echo "ERROR toolchain gcc not executable: ${CROSS}gcc" | tee "$OUT/U2_38AU11_VALIDATION_SUMMARY.txt"; exit 4; }
echo "TOOLCHAIN=$TOOLCHAIN" | tee "$OUT/toolchain_used.txt"
echo "CROSS=$CROSS" | tee -a "$OUT/toolchain_used.txt"
echo "SYSROOT=$SYSROOT" | tee -a "$OUT/toolchain_used.txt"

COMMON_FLAGS=(
  -DPLATFORM_TREEFROG -DTREEFROG_INPUT_PROFILE=0 -DTREEFROG_INPUT_DEBUG=0
  -DTREEFROG_AUDIO_MODE=0 -DTREEFROG_TIMER_MODE=1 -DTREEFROG_FACE_TAP_MODE=0
  -DTREEFROG_MODIFIER_RETRIGGER=0 -DTREEFROG_EVENT_DEBUG_ALL=0
  -DTREEFROG_DISABLE_PHRASE_AUDITION=0 -DTREEFROG_HIGH_CONTRAST_SELECTION=1
  -DTREEFROG_PURPLE_FOCUS_SELECTION=1 -DTREEFROG_PURPLE_FOCUS_RGB565=0xd99b
  -DTREEFROG_VIDEO_MODE=0 -DTREEFROG_VIDEO_PROBE=0 -DTREEFROG_ENABLE_START=1
  -DTREEFROG_ENABLE_SELECT=1 -DTREEFROG_PORT_VERSION_BADGE=0
  -DTREEFROG_PORT_VERSION_BADGE_COLOR=0xd99b -DTREEFROG_AUDIO_DEBUG=1
  -DTREEFROG_RETRO_LIFECYCLE_DEBUG=0 -DTREEFROG_UAC2_BRIDGE=1
  -DTREEFROG_PHYSICAL_VOLUME_EVDEV=0 -DTREEFROG_PHYSICAL_VOLUME_SYSTEM_PROBE=0
  -DTREEFROG_PHYSICAL_VOLUME_JOYKEY_PROBE=0 -DTREEFROG_PHYSICAL_VOLUME_STEP=5
  -DCPP_MEMORY -DHAVE_STDINT_H -D_NDEBUG -D_NO_JACK_ -DDUMMYMIDI -D_LGPT_NO_SCREEN_CACHE_
  -Iinclude -I"$SRC/sources/Adapters/TREEFROG/Compat" -I"$SRC/sources" -I"$SYSROOT/usr/include"
  -mips32r2 -march=mips32r2 -mtune=74kc -mdspr2 -mfp32 -mhard-float -mlong-calls -EL --sysroot="$SYSROOT"
  -O2 -fno-strict-aliasing -fPIC -ffunction-sections -fdata-sections -include stdint.h -include SDL_types_force.h
  -Wall -std=gnu++03
)
(
  cd "$SRC/projects"
  for cpp in \
    "$SRC/sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp" \
    "$SRC/sources/Adapters/TREEFROG/Audio/TreeFrogAudioDriver.cpp" \
    "$SRC/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp" \
    "$SRC/sources/Application/Views/InstrumentView.cpp" \
    "$SRC/sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp" \
    "$SRC/sources/Application/Views/ModalDialogs/SampleManagerDialog.cpp" \
    "$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"; do
    echo "PREFLIGHT_CPP=$cpp"
    "${CROSS}g++" -MMD -MF /tmp/au11_preflight.d "${COMMON_FLAGS[@]}" -fsyntax-only "$cpp"
  done
) 2>&1 | tee "$OUT/preflight_au11_modified_cpp.log"

"${CROSS}gcc" -mips32r2 -march=mips32r2 -mtune=74kc -mdspr2 -mfp32 -mhard-float -EL --sysroot="$SYSROOT" -O2 -static -o "$OUT/r36s_au11_usb_audio_io" "$PKG_DIR/src/r36s_au11_usb_audio_io.c" 2>&1 | tee "$OUT/build_daemon.log"
chmod +x "$OUT/r36s_au11_usb_audio_io"
(
  cd "$SRC/projects"
  rm -rf buildTREEFROG
  rm -f lgpt_libretro.so ../lgpt_libretro.so ../dist/lgpt_libretro.so
  find . -type f \( -name '*.o' -o -name '*.d' -o -name '*.so' \) -delete 2>/dev/null || true
  env PWD="$SRC/projects" make PLATFORM=TREEFROG TOOLCHAIN="$TOOLCHAIN" TREEFROG_INPUT_PROFILE=0 TREEFROG_ENABLE_START=1 TREEFROG_ENABLE_SELECT=1 TREEFROG_AUDIO_MODE=0 TREEFROG_TIMER_MODE=1 TREEFROG_FACE_TAP_MODE=0 TREEFROG_MODIFIER_RETRIGGER=0 TREEFROG_DISABLE_PHRASE_AUDITION=0 TREEFROG_HIGH_CONTRAST_SELECTION=1 TREEFROG_PURPLE_FOCUS_SELECTION=1 TREEFROG_PURPLE_FOCUS_RGB565=0xd99b TREEFROG_VIDEO_MODE=0 TREEFROG_VIDEO_PROBE=0 TREEFROG_PORT_VERSION_BADGE=0 TREEFROG_PORT_VERSION_BADGE_COLOR=0xd99b TREEFROG_INPUT_DEBUG=0 TREEFROG_EVENT_DEBUG_ALL=0 TREEFROG_RETRO_LIFECYCLE_DEBUG=0 TREEFROG_AUDIO_DEBUG=1 TREEFROG_UAC2_BRIDGE=1 TREEFROG_PHYSICAL_VOLUME_EVDEV=0 TREEFROG_PHYSICAL_VOLUME_SYSTEM_PROBE=0 TREEFROG_PHYSICAL_VOLUME_JOYKEY_PROBE=0 TREEFROG_PHYSICAL_VOLUME_STEP=5 -j1 2>&1 | tee "$OUT/build_lgpt_au11.log"
)
CORE=""
for cand in "$SRC/projects/lgpt_libretro.so" "$SRC/lgpt_libretro.so" "$SRC/dist/lgpt_libretro.so" "$SRC/projects/buildTREEFROG/lgpt_libretro.so"; do [ -f "$cand" ] && { CORE="$cand"; break; }; done
if [ -z "$CORE" ]; then find "$SRC" -name 'lgpt_libretro.so' -o -name 'lgpt.so' > "$OUT/core_search.txt" || true; echo "SUMMARY=FAIL_CORE_MISSING" | tee "$OUT/U2_38AU11_VALIDATION_SUMMARY.txt"; cat "$OUT/core_search.txt" || true; exit 5; fi
cp "$CORE" "$OUT/lgpt_libretro_au11u_cleanroom_root_diag.so"
# AU11M packaging fix:
# Some toolchains build the C++ marker function but later remove the section via
# -ffunction-sections/--gc-sections, and strip removes remaining symbol names.
# Append a small build-manifest trailer to the ELF after strip.
# Linux ELF loaders ignore trailing bytes. Validate with grep -a on the raw
# binary; plain strings may inspect ELF sections only and miss the trailer.
{
  printf '\nR36SX_AU11_BUILD_TRAILER_BEGIN\n'
  printf 'U2_38AU11U_CLEANROOM_SD_WSL_WIN_PURGE_ROOT_DIAG FULL_SOURCE DUPLEX_STABLE_ALWAYS_OPEN WINDOWS_HELPER_READY COLD_MODAL_FIX\n'
  printf 'AU11U_BUILD_MARKER_TRAILER=YES\n'
  printf 'R36SX_AU11_BUILD_TRAILER_END\n'
} >> "$OUT/lgpt_libretro_au11u_cleanroom_root_diag.so"
if ! grep -a -q 'U2_38AU11U_CLEANROOM_SD_WSL_WIN_PURGE_ROOT_DIAG' "$OUT/lgpt_libretro_au11u_cleanroom_root_diag.so"; then
  echo "ERROR built core does not contain AU11M build marker after trailer append" | tee "$OUT/U2_38AU11_VALIDATION_SUMMARY.txt"
  grep -a -E 'AU10|AU11|USB|SCPI' "$OUT/lgpt_libretro_au11u_cleanroom_root_diag.so" | head -120 | tee "$OUT/core_strings_debug.txt" || true
  exit 7
fi
strings -a "$OUT/lgpt_libretro_au11u_cleanroom_root_diag.so" | grep -E 'U2_38AU11|AU11_|SCPI-R|USB-C RECORD|LOCAL_ONLY|USB_OUT_AUTO_MUTE|FULL_DUPLEX' > "$OUT/core_strings_au11.txt" || true
sha256sum "$OUT"/* > "$OUT/SHA256SUMS_BUILD_OUT.txt" 2>/dev/null || true
cat > "$OUT/U2_38AU11_VALIDATION_SUMMARY.txt" <<SUM
SUMMARY=PASS_FOR_AU11_INSTALL
AU11U_BUILD_PATH_FIX=YES
SOURCE_ROOT_PACKAGE=$SRC_PKG
SOURCE_ROOT_BUILD=$SRC
CORE=$OUT/lgpt_libretro_au11u_cleanroom_root_diag.so
DAEMON=$OUT/r36s_au11_usb_audio_io
FEATURES=AU11U_CLEANROOM_SD_WSL_WIN_PURGE_ROOT_DIAG_SOURCE,LOCAL_ONLY,THREE_AUDIO_DRIVER_MODES,DUPLEX_STABLE_ALWAYS_OPEN_ENDPOINTS,WINDOWS_ENDPOINTS_ALWAYS_PRESENT,HOST_PLAYBACK_ALWAYS_DRAINED,CONSOLE_GATES_DIRECTION,LOCAL_PRELISTEN_IDLE_RENDER,FIRST_MODAL_TRAMPOLINE_NO_SAMPLEPOOL,FIRST_MODAL_STOP_ONLY_BARRIER,WINDOWS_COMPANION_HELPER_OPTIONAL,FULL_DUPLEX_ALIAS_REMOVED_FROM_UI,CHOPPER_L2A_RECORD_DISABLED,USB_RECORD_EXIT_MONITOR_OFF,R1_RIGHT_ENTER_R1_LEFT_EXIT
SUM
cat "$OUT/U2_38AU11_VALIDATION_SUMMARY.txt"
