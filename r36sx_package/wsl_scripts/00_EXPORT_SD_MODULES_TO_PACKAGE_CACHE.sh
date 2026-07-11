#!/usr/bin/env bash
set -euo pipefail
DRIVE="${1:-F}"; DRIVE="${DRIVE%:}"
SD="/mnt/${DRIVE,,}"
PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
[ -d "$SD" ] || { echo "ERROR SD mount not found: $SD"; exit 4; }
if [ ! -d "$SD/lgpt/otg/modules" ]; then
  echo "ERROR_MODULES_DIR_NOT_FOUND=$SD/lgpt/otg/modules"
  exit 5
fi
if ! find "$SD/lgpt/otg/modules" -type f -name '*.ko' 2>/dev/null | grep -q .; then
  echo "ERROR_MODULES_DIR_HAS_NO_KO=$SD/lgpt/otg/modules"
  exit 6
fi
mkdir -p "$PKG_DIR/modules_cache"
rm -rf "$PKG_DIR/modules_cache/modules"
cp -r --no-preserve=all "$SD/lgpt/otg/modules" "$PKG_DIR/modules_cache/modules" 2>/dev/null || cp -r "$SD/lgpt/otg/modules" "$PKG_DIR/modules_cache/modules"
find "$PKG_DIR/modules_cache/modules" -type f -name '*.ko' | wc -l | sed 's/^/EXPORTED_KO_COUNT=/'
echo "SUMMARY=PASS_AU11Z4_EXPORT_SD_MODULES_TO_PACKAGE_CACHE"
