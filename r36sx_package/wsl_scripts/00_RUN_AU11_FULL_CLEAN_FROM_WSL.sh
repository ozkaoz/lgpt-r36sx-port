#!/usr/bin/env bash
set -euo pipefail
PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "$PKG_DIR/bin/00_RUN_AU11Z_FULL_CLEAN_FROM_WSL.sh" "$@"
