#!/usr/bin/env bash
set -euo pipefail
MODE_RAW="${1:-list}"
case "${MODE_RAW,,}" in
  remove|--remove) MODE="Remove" ;;
  list|--list) MODE="List" ;;
  skip|--skip|none) MODE="Skip" ;;
  *) echo "ERROR mode must be list, remove, or skip. Got: $MODE_RAW"; exit 2 ;;
esac
PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PS1="$PKG_DIR/tools/windows-clean/R36SX_CLEAN_PRETEST_AUDIO_DEVICE_CACHE.ps1"
[ -f "$PS1" ] || { echo "ERROR missing $PS1"; exit 3; }
echo "AU11Z_WINDOWS_R36SX_DEVICE_CLEAN_START"
echo "MODE=$MODE"
if ! command -v powershell.exe >/dev/null 2>&1; then
  echo "WARN_POWERSHELL_EXE_NOT_FOUND_IN_WSL=YES"
  echo "SUMMARY=SKIP_WINDOWS_CLEAN_NO_POWERSHELL"
  exit 0
fi
WIN_PS1="$(wslpath -w "$PS1")"
# Removal requires an elevated Windows shell. From normal WSL this will usually list/fail-safe.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$WIN_PS1" -Mode "$MODE" -RestartAudio
