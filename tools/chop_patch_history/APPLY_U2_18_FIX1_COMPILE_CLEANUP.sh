#!/usr/bin/env bash
# U2.18 FIX1: compile fix for SampleChopperModal.cpp.
# Removes a stale cleanupInvalidPhraseChopNotesForCurrentSample() call that was
# referenced by U2.18 but not implemented in the current chopper code path.
# Apply on top of U2.18 after the compile error shown by the user.
set -eu
SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"

cd "$SRC" || { echo "ERROR: cannot enter SRC=$SRC"; exit 2; }

CPP="sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
H="sources/Application/Views/ModalDialogs/SampleChopperModal.h"
test -f "$CPP" || { echo "ERROR: missing $CPP"; exit 3; }
test -f "$H" || { echo "ERROR: missing $H"; exit 3; }

if ! grep -q "destructiveCropToSelectedRange" "$CPP"; then
  echo "ERROR: destructive crop code not found. Apply U2.18 first."
  exit 4
fi

BACKUP="_backup_before_u2_18_fix1_compile_cleanup_$STAMP.tar.gz"
tar -czf "$BACKUP" "$CPP" "$H" projects/Makefile

echo "Backup written: $SRC/$BACKUP"

python3 - <<'PY'
from pathlib import Path
p = Path('sources/Application/Views/ModalDialogs/SampleChopperModal.cpp')
s = p.read_text()
needle = '    cleanupInvalidPhraseChopNotesForCurrentSample();\n'
if needle in s:
    s = s.replace(needle, '    /* U2.18 FIX1: no phrase cleanup call here; crop resets this WAV to a normal shortened sample. */\n', 1)
    p.write_text(s)
    print('U2.18 FIX1 applied: removed stale cleanupInvalidPhraseChopNotesForCurrentSample() call.')
else:
    print('U2.18 FIX1: stale cleanup call already absent; continuing build.')
PY

rm -f projects/buildTREEFROG/*.o projects/buildTREEFROG/*.d dist/lgpt_libretro.so

LOG="BUILD_U2_18_FIX1_COMPILE_CLEANUP_$STAMP.log"
echo "Starting U2.18 FIX1 build..."
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh 2>&1 | tee "$LOG"
RC=${PIPESTATUS[0]}

echo
echo "BUILD_RC=$RC"
echo "LOG=$SRC/$LOG"
if [ "$RC" -eq 0 ]; then
  ls -lh "$SRC/dist/lgpt_libretro.so"
  sha256sum "$SRC/dist/lgpt_libretro.so"
else
  echo "Build failed. Relevant errors:"
  grep -nE "error:|undefined reference|No such file|fatal error|make.*Error|Error [0-9]+" "$LOG" | sed -n '1,320p'
  tail -n 200 "$LOG"
fi
exit "$RC"
