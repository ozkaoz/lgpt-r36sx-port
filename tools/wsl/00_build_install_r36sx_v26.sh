#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="${1:-/mnt/d/R36S/PORT LPTRACKER}"
SD_LETTER="${2:-}"
BUILD_DIR="${3:-/tmp/lgpt_r36sx_v26}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [[ -z "$SD_LETTER" ]]; then
  echo "Uso: bash tools/wsl/00_build_install_r36sx_v26.sh \"/mnt/d/R36S/PORT LPTRACKER\" F /tmp/lgpt_r36sx_v26" >&2
  echo "Cambia F por la letra real de la SD en Windows." >&2
  exit 2
fi

SD_LETTER="${SD_LETTER%:}"
SD="/mnt/${SD_LETTER,,}"
if [[ ! -d "$SD" ]]; then
  echo "ERROR: No existe $SD. Monta la SD en Windows y vuelve a intentar." >&2
  exit 1
fi
if [[ ! -d "$SD/cubegm" || ! -d "$SD/frogui" ]]; then
  echo "ERROR: La SD no parece tener Stock OS + TreeFrogUI. Faltan cubegm/ o frogui/." >&2
  exit 1
fi
mkdir -p "$SD/cubegm/cores" "$SD/roms/lgpt" "$SD/lgpt"

case "$BUILD_DIR" in
  *[[:space:]]*) echo "ERROR: BUILD_DIR no puede contener espacios: $BUILD_DIR" >&2; exit 2;;
esac

find_toolchain() {
  local cand gcc_path
  if [[ -n "${TOOLCHAIN:-}" ]]; then
    if [[ -x "$TOOLCHAIN/bin/mips-mti-linux-gnu-gcc" || -x "$TOOLCHAIN/opt/ext-toolchain/bin/mips-mti-linux-gnu-gcc" ]]; then
      printf '%s\n' "$TOOLCHAIN"; return 0
    fi
  fi
  for cand in \
    "$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot" \
    "/home/dafunknoise/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot" \
    "$BASE_DIR/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot"; do
    if [[ -x "$cand/bin/mips-mti-linux-gnu-gcc" || -x "$cand/opt/ext-toolchain/bin/mips-mti-linux-gnu-gcc" ]]; then
      printf '%s\n' "$cand"; return 0
    fi
  done
  gcc_path="$(find "$HOME" "$BASE_DIR" /mnt/d/R36S -maxdepth 9 -type f -name mips-mti-linux-gnu-gcc 2>/dev/null | head -1 || true)"
  if [[ -n "$gcc_path" ]]; then
    case "$gcc_path" in
      */bin/mips-mti-linux-gnu-gcc) printf '%s\n' "${gcc_path%/bin/mips-mti-linux-gnu-gcc}"; return 0;;
      */opt/ext-toolchain/bin/mips-mti-linux-gnu-gcc) printf '%s\n' "${gcc_path%/opt/ext-toolchain/bin/mips-mti-linux-gnu-gcc}"; return 0;;
    esac
  fi
  return 1
}

TOOLCHAIN="$(find_toolchain || true)"
if [[ -z "$TOOLCHAIN" ]]; then
  cat >&2 <<MSG
ERROR: no encontré el toolchain MIPS.
No descargues nada todavía. Primero busca si existe en tu PC:
  find /home/dafunknoise /mnt/d/R36S -name mips-mti-linux-gnu-gcc 2>/dev/null | head

Si aparece, define TOOLCHAIN así:
  export TOOLCHAIN=/ruta/al/mipsel-buildroot-linux-gnu_sdk-buildroot
MSG
  exit 4
fi

if [[ -x "$TOOLCHAIN/bin/mips-mti-linux-gnu-gcc" ]]; then
  CROSS="$TOOLCHAIN/bin/mips-mti-linux-gnu-"
else
  CROSS="$TOOLCHAIN/opt/ext-toolchain/bin/mips-mti-linux-gnu-"
fi
SYSROOT="${SYSROOT:-$TOOLCHAIN/mipsel-buildroot-linux-gnu/sysroot}"
[[ -x "${CROSS}gcc" ]] || { echo "ERROR: gcc no ejecutable: ${CROSS}gcc" >&2; exit 4; }

OUT="$BUILD_DIR/U2_38AU11_BUILD_OUT"
SRC_PARENT="$BUILD_DIR/source_build"
SRC="$SRC_PARENT/lgpt-r36sx-port"
rm -rf "$BUILD_DIR"
mkdir -p "$OUT" "$SRC_PARENT"

# Copia a una ruta temporal sin espacios. Esto evita que GNU make rompa include $(PWD)/Makefile.TREEFROG.
echo "Copiando fuente a ruta temporal sin espacios: $SRC"
(
  cd "$ROOT"
  tar \
    --exclude='./.git' \
    --exclude='./dist' \
    --exclude='./projects/buildTREEFROG' \
    --exclude='./projects/*.o' \
    --exclude='./projects/*.d' \
    --exclude='./*.so' \
    -cf - .
) | (
  mkdir -p "$SRC"
  cd "$SRC"
  tar -xf -
)

mkdir -p "$SRC/dist"

cat > "$OUT/build_context.txt" <<CTX
ROOT=$ROOT
BASE_DIR=$BASE_DIR
BUILD_DIR=$BUILD_DIR
SOURCE_BUILD=$SRC
SD=$SD
TOOLCHAIN=$TOOLCHAIN
CROSS=$CROSS
SYSROOT=$SYSROOT
CTX
cat "$OUT/build_context.txt"

COMMON_MAKE_VARS=(
  PLATFORM=TREEFROG
  TOOLCHAIN="$TOOLCHAIN"
  TREEFROG_INPUT_PROFILE=0
  TREEFROG_ENABLE_START=1
  TREEFROG_ENABLE_SELECT=1
  TREEFROG_AUDIO_MODE=0
  TREEFROG_TIMER_MODE=1
  TREEFROG_FACE_TAP_MODE=0
  TREEFROG_MODIFIER_RETRIGGER=0
  TREEFROG_DISABLE_PHRASE_AUDITION=0
  TREEFROG_HIGH_CONTRAST_SELECTION=1
  TREEFROG_PURPLE_FOCUS_SELECTION=1
  TREEFROG_PURPLE_FOCUS_RGB565=0xd99b
  TREEFROG_VIDEO_MODE=0
  TREEFROG_VIDEO_PROBE=0
  TREEFROG_PORT_VERSION_BADGE=0
  TREEFROG_PORT_VERSION_BADGE_COLOR=0xd99b
  TREEFROG_INPUT_DEBUG=0
  TREEFROG_EVENT_DEBUG_ALL=0
  TREEFROG_RETRO_LIFECYCLE_DEBUG=0
  TREEFROG_AUDIO_DEBUG=1
  TREEFROG_UAC2_BRIDGE=1
  TREEFROG_PHYSICAL_VOLUME_EVDEV=0
  TREEFROG_PHYSICAL_VOLUME_SYSTEM_PROBE=0
  TREEFROG_PHYSICAL_VOLUME_JOYKEY_PROBE=0
  TREEFROG_PHYSICAL_VOLUME_STEP=5
)

DAEMON_SRC="$SRC/r36sx_package/source_overrides/r36s_au11_usb_audio_io.c"
if [[ -f "$DAEMON_SRC" ]]; then
  echo "Compilando daemon AU11: $DAEMON_SRC"
  "${CROSS}gcc" -mips32r2 -march=mips32r2 -mtune=74kc -mdspr2 -mfp32 -mhard-float -EL --sysroot="$SYSROOT" \
    -O2 -static -o "$OUT/r36s_au11_usb_audio_io" "$DAEMON_SRC" 2>&1 | tee "$OUT/build_daemon.log"
  chmod +x "$OUT/r36s_au11_usb_audio_io"
else
  echo "WARN: no existe $DAEMON_SRC. Se compilará solo el core." | tee "$OUT/build_daemon.log"
fi

(
  cd "$SRC/projects"
  rm -rf buildTREEFROG
  rm -f lgpt_libretro.so ../lgpt_libretro.so ../dist/lgpt_libretro.so lgpt.so ../lgpt.so
  find . -type f \( -name '*.o' -o -name '*.d' -o -name '*.so' \) -delete 2>/dev/null || true
  echo "Compilando LGPT con Makefile principal: make PLATFORM=TREEFROG"
  env PWD="$SRC/projects" make "${COMMON_MAKE_VARS[@]}" -j1 2>&1 | tee "$OUT/build_lgpt_treefrog.log"
)

CORE=""
for cand in \
  "$SRC/projects/lgpt_libretro.so" \
  "$SRC/lgpt_libretro.so" \
  "$SRC/dist/lgpt_libretro.so" \
  "$SRC/projects/buildTREEFROG/lgpt_libretro.so" \
  "$SRC/projects/lgpt.so" \
  "$SRC/lgpt.so"; do
  if [[ -f "$cand" ]]; then CORE="$cand"; break; fi
done

if [[ -z "$CORE" ]]; then
  find "$SRC" -name 'lgpt_libretro.so' -o -name 'lgpt.so' > "$OUT/core_search.txt" || true
  echo "ERROR: no se generó lgpt_libretro.so/lgpt.so. Revisa $OUT/build_lgpt_treefrog.log" >&2
  cat "$OUT/core_search.txt" >&2 || true
  exit 5
fi

cp -f "$CORE" "$OUT/lgpt_libretro_au11z7.so"
{
  printf '\nR36SX_AU11Z7_BUILD_TRAILER_BEGIN\n'
  printf 'R36SX_V2_6_TREEFROGUI_LGPT_AU11Z7 START_LGPT_ROOT_LGPT WSL24_SPACE_PATH_FIX MAKE_PLATFORM_TREEFROG\n'
  printf 'R36SX_AU11Z7_BUILD_TRAILER_END\n'
} >> "$OUT/lgpt_libretro_au11z7.so"
cp -f "$OUT/lgpt_libretro_au11z7.so" "$SRC/dist/lgpt_libretro.so"
mkdir -p "$ROOT/dist"
cp -f "$OUT/lgpt_libretro_au11z7.so" "$ROOT/dist/lgpt_libretro.so"
sha256sum "$OUT/lgpt_libretro_au11z7.so" > "$OUT/SHA256SUMS_BUILD_OUT.txt"

safe_cp_file() {
  local src="$1" dst="$2"
  mkdir -p "$(dirname "$dst")" 2>/dev/null || true
  cp -f --no-preserve=all "$src" "$dst" 2>/dev/null || cp -f "$src" "$dst"
}
write_text() {
  local dst="$1" text="$2"
  mkdir -p "$(dirname "$dst")" 2>/dev/null || true
  rm -f "$dst" 2>/dev/null || true
  printf '%s\n' "$text" > "$dst"
}

LGPT_ROOT="$SD/lgpt"
mkdir -p "$LGPT_ROOT/projects" "$LGPT_ROOT/samples" "$LGPT_ROOT/instruments" "$LGPT_ROOT/images" "$LGPT_ROOT/exports" "$LGPT_ROOT/chops" "$LGPT_ROOT/tmp" "$LGPT_ROOT/backups" "$LGPT_ROOT/otg/bin" "$LGPT_ROOT/otg/logs" "$SD/roms/lgpt" "$SD/cubegm/cores"

safe_cp_file "$OUT/lgpt_libretro_au11z7.so" "$SD/cubegm/cores/lgpt_libretro.so"
safe_cp_file "$OUT/lgpt_libretro_au11z7.so" "$SD/cubegm/lgpt_libretro.so"

HANDLER="$ROOT/installers/stock_treefrogui_clean/LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER/handler/lgpt"
CONFIG="$ROOT/installers/stock_treefrogui_clean/LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER/runtime/config.xml"
if [[ -f "$HANDLER" ]]; then
  safe_cp_file "$HANDLER" "$SD/cubegm/lgpt"
  safe_cp_file "$HANDLER" "$SD/cubegm/lgpt.elf"
else
  cat > "$SD/cubegm/lgpt" <<'HANDLER'
#!/bin/sh
ROOT="/mnt/sdcard"
PICO="$ROOT/cubegm/picoarch"
CORE="$ROOT/cubegm/cores/lgpt_libretro.so"
ROM="${1:-$ROOT/roms/lgpt/start.lgpt}"
LGPT_HOME="$ROOT/lgpt"
mkdir -p "$LGPT_HOME" "$LGPT_HOME/projects" "$LGPT_HOME/samples" "$LGPT_HOME/instruments" "$ROOT/roms/lgpt"
cd "$LGPT_HOME" || exit 1
exec "$PICO" "$CORE" "$ROM"
HANDLER
  safe_cp_file "$SD/cubegm/lgpt" "$SD/cubegm/lgpt.elf"
fi
chmod +x "$SD/cubegm/lgpt" "$SD/cubegm/lgpt.elf" 2>/dev/null || true

if [[ -f "$CONFIG" ]]; then
  safe_cp_file "$CONFIG" "$LGPT_ROOT/config.xml"
else
  cat > "$LGPT_ROOT/config.xml" <<'XML'
<CONFIG>
  <ROOTFOLDER value="/mnt/sdcard/lgpt" />
  <SAMPLELIB value="/mnt/sdcard/lgpt/samples" />
  <INSTRUMENTFOLDER value="/mnt/sdcard/lgpt/instruments" />
</CONFIG>
XML
fi

write_text "$SD/roms/lgpt/start.lgpt" "LGPT_START"
write_text "$SD/roms/lgpt/filelist.csv" "start.lgpt,LGPT,LGPT"
cat > "$SD/roms/lgpt/README_LGPT_R36SX.txt" <<'TXT'
LGPT R36SX v2.6 TreeFrogUI
Entrada visible: roms/lgpt/start.lgpt
Launcher: cubegm/lgpt
Core: cubegm/cores/lgpt_libretro.so
Datos: /lgpt
TXT

if [[ -f "$OUT/r36s_au11_usb_audio_io" ]]; then
  safe_cp_file "$OUT/r36s_au11_usb_audio_io" "$LGPT_ROOT/otg/bin/r36s_au11_usb_audio_io"
fi
for s in "$ROOT"/r36sx_package/device_scripts/otg_38au11_*.sh; do
  [[ -f "$s" ]] && safe_cp_file "$s" "$LGPT_ROOT/otg/bin/$(basename "$s")"
done
chmod +x "$LGPT_ROOT/otg/bin"/* 2>/dev/null || true

cat > "$LGPT_ROOT/AU11Z7_INSTALL_INFO.txt" <<INFO
INSTALLED=YES
DATE=$(date --iso-8601=seconds)
CORE=/cubegm/cores/lgpt_libretro.so
CORE_MIRROR=/cubegm/lgpt_libretro.so
LAUNCHER=/cubegm/lgpt
LAUNCHER_MIRROR=/cubegm/lgpt.elf
VISIBLE_ENTRY=/roms/lgpt/start.lgpt
DATA_ROOT=/lgpt
BUILD_OUT=$OUT
SOURCE_BUILD=$SRC
TOOLCHAIN=$TOOLCHAIN
MAKE=make PLATFORM=TREEFROG
FIXES=space_path_fix,correct_make_entry,start_lgpt,root_lgpt
INFO

sync || true

echo ""
echo "OK: compilación e instalación terminadas."
echo "Core local: $ROOT/dist/lgpt_libretro.so"
echo "Core SD:    $SD/cubegm/cores/lgpt_libretro.so"
echo "Entrada:    $SD/roms/lgpt/start.lgpt"
echo "Datos:      $SD/lgpt"
echo "Log build:  $OUT/build_lgpt_treefrog.log"
