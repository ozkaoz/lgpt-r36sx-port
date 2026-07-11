#!/usr/bin/env bash
# Instala LGPT U2.36 sobre SD Stock + TreeFrogUI usando PowerShell nativo para escribir en la SD.
# Uso:
#   bash INSTALL_U2_36_STOCK_TREEFROGUI_FROM_WSL.sh F /ruta/a/lgpt_libretro.so
set -e
DRIVE_LETTER="${1:-F}"
CORE_SRC="${2:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT/dist/lgpt_libretro.so}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ ! -f "$CORE_SRC" ]; then
  echo "ERROR: no existe el core local: $CORE_SRC" >&2
  exit 1
fi

CORE_WIN="$(wslpath -w "$CORE_SRC")"
PS_WIN="$(wslpath -w "$SCRIPT_DIR/INSTALL_U2_36_STOCK_TREEFROGUI.ps1")"
CFG_WIN="$(wslpath -w "$SCRIPT_DIR/runtime/config.xml")"
HANDLER_WIN="$(wslpath -w "$SCRIPT_DIR/handler/lgpt")"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PS_WIN" -DriveLetter "$DRIVE_LETTER" -CorePath "$CORE_WIN" -ConfigPath "$CFG_WIN" -HandlerPath "$HANDLER_WIN"
