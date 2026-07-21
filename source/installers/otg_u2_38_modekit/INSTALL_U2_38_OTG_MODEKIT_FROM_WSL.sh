#!/usr/bin/env bash
set -euo pipefail
DRIVE_LETTER="${1:-F}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PS_WIN="$(wslpath -w "$SCRIPT_DIR/INSTALL_U2_38_OTG_MODEKIT.ps1")"
SRC_WIN="$(wslpath -w "$ROOT_DIR/scripts")"
DOCS_WIN="$(wslpath -w "$ROOT_DIR/docs")"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PS_WIN" -DriveLetter "$DRIVE_LETTER" -SourceDir "$SRC_WIN" -DocsDir "$DOCS_WIN"
