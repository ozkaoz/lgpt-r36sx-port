#!/usr/bin/env bash
# Wrapper WSL para instalar LGPT U2.36 en una SD TreeFrogUI.
# Uso:
#   bash INSTALL_U2_36_LGPT_TREEFROGUI_FROM_WSL.sh F /home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT/dist/lgpt_libretro.so
# Si omites argumentos usa F: y el SRC estándar.

set -u
DRIVE_LETTER="${1:-F}"
DRIVE_LETTER="${DRIVE_LETTER%:}"
CORE_PATH="${2:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT/dist/lgpt_libretro.so}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PS1_PATH="$SCRIPT_DIR/INSTALL_U2_36_LGPT_TREEFROGUI.ps1"

if [ ! -f "$PS1_PATH" ]; then
  echo "ERROR: falta $PS1_PATH"
  exit 1
fi

if [ ! -f "$CORE_PATH" ]; then
  echo "ERROR: no existe el core local: $CORE_PATH"
  echo "Compila primero U2.36 o pasa la ruta del core como segundo argumento."
  exit 1
fi

WIN_CORE="$(wslpath -w "$CORE_PATH")"
WIN_PS1="$(wslpath -w "$PS1_PATH")"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$WIN_PS1" -Drive "${DRIVE_LETTER}:" -CorePath "$WIN_CORE"
