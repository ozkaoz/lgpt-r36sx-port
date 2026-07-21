#!/usr/bin/env bash
set -euo pipefail
DRIVE="${1:-F}"
OUT="${2:-$(pwd)}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WIN_SCRIPT="$(wslpath -w "$SCRIPT_DIR/COLLECT_U2_39A_OTG_LOGS.ps1")"
WIN_OUT="$(wslpath -w "$OUT")"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$WIN_SCRIPT" -DriveLetter "$DRIVE" -OutputDir "$WIN_OUT"
