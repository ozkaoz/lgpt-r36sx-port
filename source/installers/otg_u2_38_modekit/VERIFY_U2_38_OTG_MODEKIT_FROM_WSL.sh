#!/usr/bin/env bash
set -euo pipefail
DRIVE_LETTER="${1:-F}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PS_WIN="$(wslpath -w "$SCRIPT_DIR/VERIFY_U2_38_OTG_MODEKIT.ps1")"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PS_WIN" -DriveLetter "$DRIVE_LETTER"
