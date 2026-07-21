#!/usr/bin/env bash
set -euo pipefail
DRIVE="${1:-F}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WIN_SCRIPT="$(wslpath -w "$SCRIPT_DIR/VERIFY_U2_39A_OTG_KERNEL_MODULE_PREP.ps1")"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$WIN_SCRIPT" -DriveLetter "$DRIVE"
