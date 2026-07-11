#!/usr/bin/env bash
set -euo pipefail
cat <<'MSG'
AU11Z_SAFE_GUARD=YES
This destructive legacy deep-clean script has been disabled in AU11Z because older versions removed /lgpt/otg/modules and broke USB Audio enumeration.
Delegating to the safe cleanroom preinstall that preserves/restores modules.
MSG
PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "$PKG_DIR/bin/00_CLEANROOM_SD_WSL_PREINSTALL_AU11Z.sh" "$@"
