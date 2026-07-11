#!/usr/bin/env bash
set -e
DRIVE_LETTER="${1:-F}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PS_WIN="$(wslpath -w "$SCRIPT_DIR/VERIFY_U2_36_STOCK_TREEFROGUI.ps1")"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PS_WIN" -DriveLetter "$DRIVE_LETTER"
