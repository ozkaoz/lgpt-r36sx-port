#!/usr/bin/env bash
set -euo pipefail
DRIVE_LETTER="${1:-F}"
OUTPUT_DIR="${2:-$(pwd)}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PS_WIN="$(wslpath -w "$SCRIPT_DIR/COLLECT_U2_38_OTG_LOGS.ps1")"
OUT_WIN="$(wslpath -w "$OUTPUT_DIR")"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PS_WIN" -DriveLetter "$DRIVE_LETTER" -OutputDir "$OUT_WIN"
