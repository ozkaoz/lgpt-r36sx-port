#!/usr/bin/env bash
set -euo pipefail
PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
echo "AU11Y_COMPAT_WRAPPER=AU11Z_CAMILO_SAFE_CLEANROOM"
exec "$PKG_DIR/bin/00_CLEANROOM_SD_WSL_PREINSTALL_AU11Z.sh" "$@"
