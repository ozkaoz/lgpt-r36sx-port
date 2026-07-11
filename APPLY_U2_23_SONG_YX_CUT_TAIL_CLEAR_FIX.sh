#!/usr/bin/env bash
set -euo pipefail

SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"
SONG="$SRC/sources/Application/Views/SongView.cpp"
BACKUP_DIR="$SRC/backups/U2_23_SONG_YX_CUT_TAIL_CLEAR_$STAMP"
LOG="$SRC/BUILD_U2_23_SONG_YX_CUT_TAIL_CLEAR_$STAMP.log"

fail() { echo "ERROR: $*" >&2; exit 1; }

[ -d "$SRC" ] || fail "No existe SRC: $SRC"
[ -f "$SONG" ] || fail "No existe SongView.cpp: $SONG"
[ -f "$SRC/BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh" ] || fail "No existe script de build estable en SRC"

grep -q "TREEFROG_INPUT_SONG_YX_CUT_NORMAL_MODE_V2" "$SONG" || fail "No se encontró marcador Y+X normal mode V2; revisa que estés sobre U2.22 estable"
grep -q "TREEFROG_INPUT_SONG_AB_CLEAR_YX_CUT" "$SONG" || fail "No se encontró marcador A+B clear; revisa el árbol base"
grep -q "for (int j = 0; j > clipboard_.height_; j++)" "$SONG" || fail "El patrón de cola sin limpiar no existe; quizá el fix ya fue aplicado"

mkdir -p "$BACKUP_DIR"
cp -p "$SONG" "$BACKUP_DIR/SongView.cpp.before_u2_23_tail_clear"

python3 - "$SONG" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
old = "for (int j = 0; j > clipboard_.height_; j++) {"
new = "for (int j = 0; j < clipboard_.height_; j++) {"
if s.count(old) != 1:
    raise SystemExit(f"Expected exactly one old loop, found {s.count(old)}")
s = s.replace(old, "// TREEFROG_U2_23_SONG_CUT_TAIL_CLEAR_FIX\n    " + new, 1)
p.write_text(s)
PY

grep -q "TREEFROG_U2_23_SONG_CUT_TAIL_CLEAR_FIX" "$SONG" || fail "No se insertó marcador U2.23"
grep -q "for (int j = 0; j < clipboard_.height_; j++)" "$SONG" || fail "No quedó el bucle corregido"
if grep -q "for (int j = 0; j > clipboard_.height_; j++)" "$SONG"; then
  fail "Sigue existiendo el bucle defectuoso"
fi

cd "$SRC"
set +e
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh 2>&1 | tee "$LOG"
BUILD_RC=${PIPESTATUS[0]}
set -e

echo "BUILD_RC=$BUILD_RC"
[ "$BUILD_RC" -eq 0 ] || fail "Falló compilación. Log: $LOG"
[ -f "$SRC/dist/lgpt_libretro.so" ] || fail "No se generó dist/lgpt_libretro.so"
sha256sum "$SRC/dist/lgpt_libretro.so" | tee "$SRC/dist/lgpt_libretro.so.u2_23.sha256"

echo "OK: U2.23 aplicado y compilado. Backup: $BACKUP_DIR"
