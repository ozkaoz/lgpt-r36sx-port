#!/usr/bin/env bash
set -euo pipefail
SRC="${1:-$(pwd)}"
cd "$SRC"
bash ./VERIFY_U2_36_SOURCE.sh "$SRC"
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh
ls -lh "$SRC/dist/lgpt_libretro.so"
sha256sum "$SRC/dist/lgpt_libretro.so"
echo "OK: build U2.36 estable finalizado."
