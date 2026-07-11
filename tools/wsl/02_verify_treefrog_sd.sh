#!/usr/bin/env bash
set -euo pipefail
DRIVE="${1:-F}"; DRIVE="${DRIVE%:}"
SD="/mnt/${DRIVE,,}"
[[ -d "$SD" ]] || { echo "ERROR: SD no detectada: $SD" >&2; exit 1; }
echo "SD detectada: $SD"
check_dir(){ [[ -d "$SD/$1" ]] && echo "[OK] $1" || echo "[FALTA] $1"; }
check_file(){ [[ -f "$SD/$1" ]] && echo "[OK] $1" || echo "[FALTA] $1"; }
check_dir cubegm
check_dir cubegm/cores
check_dir frogui
check_dir roms
check_dir roms/lgpt
check_dir lgpt
check_file cubegm/cores/lgpt_libretro.so
check_file cubegm/lgpt_libretro.so
check_file cubegm/lgpt
check_file cubegm/lgpt.elf
check_file roms/lgpt/start.lgpt
check_file lgpt/config.xml
if [[ -f "$SD/roms/lgpt/start.lgpt" ]]; then
  echo "Contenido start.lgpt: $(tr -d '\r\n' < "$SD/roms/lgpt/start.lgpt" 2>/dev/null || true)"
else
  echo "[INFO] LGPT todavía no está instalado. Antes de instalar es normal que falten /lgpt y roms/lgpt/start.lgpt."
fi
