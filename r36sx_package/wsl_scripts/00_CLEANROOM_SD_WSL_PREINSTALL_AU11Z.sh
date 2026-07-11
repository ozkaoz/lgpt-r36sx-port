#!/usr/bin/env bash
set -euo pipefail
DRIVE="${1:-F}"; DRIVE="${DRIVE%:}"
BUILD_ROOT="${2:-/tmp/r36s_u2_38au11z4}"
WORKDIR="${3:-/mnt/d/R36S/PORT LPTRACKER}"
SD="/mnt/${DRIVE,,}"
PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
[ -d "$SD" ] || { echo "ERROR SD mount not found: $SD"; exit 4; }
TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="$SD/_AU11Z4_CLEANROOM_BACKUP_$TS"
HOST_BACKUP="$WORKDIR/_AU11Z4_HOST_CLEAN_BACKUP_$TS"
mkdir -p "$BACKUP" "$HOST_BACKUP" 2>/dev/null || true
REPORT="$BACKUP/cleanroom_report.txt"

# WSL + tarjetas FAT/exFAT/NTFS pueden rechazar cp -a/cp -p por timestamps/modos.
# En SD no necesitamos preservar metadatos Unix; necesitamos contenido íntegro y verificable.
safe_cp_file() {
  local src="$1" dst="$2"
  mkdir -p "$(dirname "$dst")" 2>/dev/null || true
  cp -f --no-preserve=all "$src" "$dst" 2>/dev/null || cp -f "$src" "$dst"
}
safe_cp_tree() {
  local src="$1" dst="$2"
  rm -rf "$dst" 2>/dev/null || true
  mkdir -p "$(dirname "$dst")" 2>/dev/null || true
  cp -r --no-preserve=all "$src" "$dst" 2>/dev/null || cp -r "$src" "$dst"
}
safe_archive_item() {
  local src="$1" dst="$2"
  if [ -d "$src" ]; then
    safe_cp_tree "$src" "$dst"
  elif [ -f "$src" ]; then
    safe_cp_file "$src" "$dst"
  fi
}

{
  echo "AU11Z4_CAMILO_CLEANROOM_START=$TS"
  echo "SD=$SD"
  echo "WORKDIR=$WORKDIR"
  echo "PKG_DIR=$PKG_DIR"
  echo "BUILD_ROOT=$BUILD_ROOT"
  echo "COPY_POLICY=NO_PRESERVE_TIMES_MODES_OWNERSHIP_FOR_WSL_SD"
} | tee "$REPORT"

# 1) Limpieza WSL/Ubuntu: solo residuos de desarrollo/runtime de este port.
rm -rf "$BUILD_ROOT" /tmp/r36sx_lgpt_usb /tmp/r36sx_uac2_bridge_fifo /tmp/au11_logs_* \
       /tmp/r36s_u2_38au11 /tmp/r36s_u2_38au11* /tmp/r36sx_au11_active_profile \
       /tmp/u2_38au11_modal_diag.log /tmp/u2_38au11_usb_audio_io_daemon.log \
       /tmp/au11_insmod.err /tmp/au11_lc.err /tmp/au11_uac2.err 2>/dev/null || true
mkdir -p "$BUILD_ROOT/U2_38AU11_BUILD_OUT"
echo "WSL_TMP_CLEANED=YES" | tee -a "$REPORT"

# 2) Mover logs/paquetes de prueba previos del host a backup para que no contaminen lectura de resultados.
if [ -d "$WORKDIR" ]; then
  shopt -s nullglob
  for f in "$WORKDIR"/U2_38AU11*_FULL_RUN_*.log "$WORKDIR"/U2_38AU11*_TEST_LOGS_*.zip "$WORKDIR"/U2_38AU11*_WINDOWS_CLEAN_*.log; do
    [ -e "$f" ] || continue
    mv -f "$f" "$HOST_BACKUP/" 2>/dev/null || safe_archive_item "$f" "$HOST_BACKUP/$(basename "$f")" || true
  done
  shopt -u nullglob
  echo "HOST_PREVIOUS_TEST_LOGS_MOVED=$HOST_BACKUP" | tee -a "$REPORT"
fi

# 3) Backup de áreas críticas SD antes de tocar nada.
for p in "$SD/lgpt/otg" "$SD/lgpt/uac2_bridge_lgpt.log" "$SD/cubegm/cores/lgpt_libretro.so" "$SD/cubegm/lgpt_libretro.so"; do
  if [ -e "$p" ]; then
    dst="$BACKUP/$(echo "$p" | sed "s#^$SD/##" | tr '/' '_')"
    safe_archive_item "$p" "$dst" || true
    echo "BACKUP=$p -> $dst" | tee -a "$REPORT"
  fi
done

# 4) Preservar módulos kernel. No son producidos por el build del core.
mkdir -p "$BACKUP/modules_candidates"
MODULE_SRC=""
if find "$SD/lgpt/otg/modules" -type f -name '*.ko' 2>/dev/null | grep -q .; then
  MODULE_SRC="$SD/lgpt/otg/modules"
  echo "MODULES_CURRENT_FOUND=$MODULE_SRC" | tee -a "$REPORT"
else
  MODULE_SRC="$(find "$SD" -path '*/lgpt/otg/modules' -type d 2>/dev/null | while read -r d; do find "$d" -type f -name '*.ko' 2>/dev/null | grep -q . && { echo "$d"; break; }; done | head -1 || true)"
  [ -n "$MODULE_SRC" ] && echo "MODULES_BACKUP_FOUND=$MODULE_SRC" | tee -a "$REPORT"
fi
if [ -n "$MODULE_SRC" ]; then
  safe_cp_tree "$MODULE_SRC" "$BACKUP/modules_preserved" || true
  mkdir -p "$PKG_DIR/modules_cache"
  rm -rf "$PKG_DIR/modules_cache/modules"
  safe_cp_tree "$MODULE_SRC" "$PKG_DIR/modules_cache/modules" || true
  find "$MODULE_SRC" -type f -name '*.ko' | wc -l | sed 's/^/MODULES_KO_COUNT=/' | tee -a "$REPORT"
else
  echo "WARN_NO_CUSTOM_ALSA_UAC2_MODULES_FOUND=YES" | tee -a "$REPORT"
fi

# 5) Reset OTG runtime sin borrar módulos preservados.
mkdir -p "$SD/lgpt/otg"
rm -rf "$SD/lgpt/otg/bin" "$SD/lgpt/otg/logs" "$SD/lgpt/otg/logs_runtime" 2>/dev/null || true
rm -f "$SD/lgpt/otg"/audio_driver_mode "$SD/lgpt/otg"/au11_usb_policy "$SD/lgpt/otg"/au11_active_usb_profile 2>/dev/null || true
rm -f "$SD/lgpt/otg"/enable_lgpt_uac2_bridge "$SD/lgpt/otg"/disable_mute_local "$SD/lgpt/otg"/lowlat_240 2>/dev/null || true
rm -f "$SD/lgpt/uac2_bridge_lgpt.log" 2>/dev/null || true
rm -f "$SD/lgpt/otg"/U2_38AU10* "$SD/lgpt/otg"/U2_38AU11*_INSTALL_INFO.txt "$SD/lgpt/otg"/U2_38AU11*_SHA256_INSTALLED.txt 2>/dev/null || true
mkdir -p "$SD/lgpt/otg/bin" "$SD/lgpt/otg/logs"
: > "$SD/lgpt/uac2_bridge_lgpt.log" 2>/dev/null || true

# 6) Restaurar módulos sin preservar tiempos/permisos.
if [ -d "$BACKUP/modules_preserved" ]; then
  rm -rf "$SD/lgpt/otg/modules"
  safe_cp_tree "$BACKUP/modules_preserved" "$SD/lgpt/otg/modules"
  echo "MODULES_RESTORED_FROM_BACKUP=YES" | tee -a "$REPORT"
elif [ -d "$PKG_DIR/modules_cache/modules" ]; then
  rm -rf "$SD/lgpt/otg/modules"
  safe_cp_tree "$PKG_DIR/modules_cache/modules" "$SD/lgpt/otg/modules"
  echo "MODULES_RESTORED_FROM_PACKAGE_CACHE=YES" | tee -a "$REPORT"
else
  echo "WARN_MODULES_NOT_RESTORED_USB_AUDIO_MAY_FAIL=YES" | tee -a "$REPORT"
fi

# 7) Saneamiento de WAV inválidos: reemplazar WAV <=64 bytes por silencio válido, no borrar samples válidos.
INVALID_DIR="$BACKUP/invalid_wavs"
mkdir -p "$INVALID_DIR"
VALID_WAV="$BACKUP/silence_100ms_48k_mono.wav"
python3 - "$VALID_WAV" <<'PY'
import sys, wave
path=sys.argv[1]
with wave.open(path,'wb') as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(48000)
    w.writeframes(b'\x00\x00' * 4800)
PY
INVALID_COUNT=0
while IFS= read -r -d '' wav; do
  size=$(stat -c '%s' "$wav" 2>/dev/null || echo 0)
  if [ "$size" -le 64 ]; then
    rel="$(echo "$wav" | sed "s#^$SD/##")"
    mkdir -p "$INVALID_DIR/$(dirname "$rel")"
    safe_cp_file "$wav" "$INVALID_DIR/$rel" || true
    safe_cp_file "$VALID_WAV" "$wav"
    echo "INVALID_WAV_REPLACED size=$size path=$wav" | tee -a "$REPORT"
    INVALID_COUNT=$((INVALID_COUNT+1))
  fi
done < <(find "$SD/lgpt" "$SD/roms" -type f -iname '*.wav' -print0 2>/dev/null || true)
echo "INVALID_WAV_REPLACED_COUNT=$INVALID_COUNT" | tee -a "$REPORT"

cat > "$SD/lgpt/otg/AU11Z_CAMILO_CLEANROOM_PREINSTALL.txt" <<INFO
AU11Z4_CAMILO_CLEANROOM_PREINSTALL=YES
DATE=$(date --iso-8601=seconds)
BACKUP=$BACKUP
HOST_BACKUP=$HOST_BACKUP
MODULE_SRC=${MODULE_SRC:-NONE}
INVALID_WAV_REPLACED_COUNT=$INVALID_COUNT
WINDOWS_CLEAN_REQUIRED=YES_RUN_FROM_00_RUN_OR_MANUAL_ADMIN
COPY_POLICY=NO_PRESERVE_TIMES_MODES_OWNERSHIP_FOR_WSL_SD
INFO

sync
echo "SUMMARY=PASS_AU11Z4_CAMILO_CLEANROOM_PREINSTALL" | tee -a "$REPORT"
