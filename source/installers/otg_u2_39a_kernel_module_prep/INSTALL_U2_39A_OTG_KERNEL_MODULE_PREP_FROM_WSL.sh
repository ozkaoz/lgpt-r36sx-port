#!/usr/bin/env bash
set -euo pipefail
DRIVE="${1:-F}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "$SCRIPT_DIR/../scripts" && pwd)"
WIN_SCRIPT="$(wslpath -w "$SCRIPT_DIR/INSTALL_U2_39A_OTG_KERNEL_MODULE_PREP.ps1")"
WIN_SRC="$(wslpath -w "$SRC_DIR")"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$WIN_SCRIPT" -DriveLetter "$DRIVE" -SourceDir "$WIN_SRC"
