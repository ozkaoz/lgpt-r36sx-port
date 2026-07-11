#!/usr/bin/env bash
set -euo pipefail

SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"
PATCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATCH_FILE="$PATCH_DIR/U2_27_CHOPPER_PREVIEW_STOP_CHOP_UI_INSTRUMENT_FIX.patch"
CPP="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
HDR="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.h"
INST="$SRC/sources/Application/Views/InstrumentView.cpp"
BACKUP_DIR="$SRC/backups/U2_27_CHOPPER_PREVIEW_STOP_CHOP_UI_INSTRUMENT_FIX_$STAMP"
LOG="$SRC/BUILD_U2_27_CHOPPER_PREVIEW_STOP_CHOP_UI_INSTRUMENT_FIX_$STAMP.log"

fail() { echo "ERROR: $*" >&2; exit 1; }

[ -d "$SRC" ] || fail "No existe SRC: $SRC"
[ -f "$CPP" ] || fail "No existe SampleChopperModal.cpp: $CPP"
[ -f "$HDR" ] || fail "No existe SampleChopperModal.h: $HDR"
[ -f "$INST" ] || fail "No existe InstrumentView.cpp: $INST"
[ -f "$PATCH_FILE" ] || fail "No existe patch: $PATCH_FILE"
[ -f "$SRC/BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh" ] || fail "No existe script de build estable en SRC"

grep -q "TREEFROG_U2_26_CHOPPER_UI_PROGRESS_PREVIEW_FIX" "$CPP" || grep -q "TREEFROG_U2_27_CHOPPER_PREVIEW_STOP_CHOP_UI_INSTRUMENT_FIX" "$CPP" || fail "U2.26 no parece aplicado; aplica/valida U2.26 antes de U2.27"
grep -q "PITCH / ENVELOPE U2.26\|PITCH/ENV U2.27" "$CPP" || fail "No encuentro UI Pitch/Env esperada; árbol inesperado"
grep -q "void SampleChopperModal::previewPitchSetting" "$CPP" || fail "No encuentro previewPitchSetting; árbol inesperado"
grep -q "void SampleChopperModal::selectChop" "$CPP" || fail "No encuentro selectChop; árbol inesperado"
grep -q "SampleChopperModal \*scm" "$INST" || fail "No encuentro integración InstrumentView -> SampleChopperModal"

mkdir -p "$BACKUP_DIR"
cp -p "$CPP" "$BACKUP_DIR/SampleChopperModal.cpp.before_u2_27"
cp -p "$HDR" "$BACKUP_DIR/SampleChopperModal.h.before_u2_27"
cp -p "$INST" "$BACKUP_DIR/InstrumentView.cpp.before_u2_27"

if grep -q "TREEFROG_U2_27_CHOPPER_PREVIEW_STOP_CHOP_UI_INSTRUMENT_FIX" "$CPP"; then
  echo "U2.27 ya parece aplicado; no reaplico patch. Continúo con build."
else
  cd "$SRC"
  patch --dry-run -p0 < "$PATCH_FILE" >/tmp/u2_27_patch_dryrun.log 2>&1 || {
    cat /tmp/u2_27_patch_dryrun.log >&2
    fail "El patch U2.27 no aplica limpio. Backup: $BACKUP_DIR"
  }
  patch -p0 < "$PATCH_FILE"
fi

grep -q "TREEFROG_U2_27_CHOPPER_PREVIEW_STOP_CHOP_UI_INSTRUMENT_FIX" "$CPP" || fail "No quedó marcador U2.27"
grep -q "Graphical Chopper U2.27" "$CPP" || fail "No quedó cabecera Graphical Chopper U2.27"
grep -q "PITCH/ENV U2.27" "$CPP" || fail "No quedó panel PITCH/ENV U2.27"
grep -q "Stop preview" "$CPP" || fail "No quedó L2+B stop preview"
grep -q "Pitch chop %02d/%02d" "$CPP" || fail "No quedó selector R2+LEFT/RIGHT de chop en Pitch/Env"
grep -q "refreshCurrentInstrumentAfterSampleEdit" "$CPP" || fail "No quedó refresh de instrumento tras edición destructiva"
grep -q "onInstrumentChange();" "$INST" || fail "No quedó refresh de InstrumentView tras cerrar Chopper"

cd "$SRC"
set +e
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh 2>&1 | tee "$LOG"
BUILD_RC=${PIPESTATUS[0]}
set -e

echo "BUILD_RC=$BUILD_RC"
[ "$BUILD_RC" -eq 0 ] || fail "Falló compilación. Log: $LOG"
[ -f "$SRC/dist/lgpt_libretro.so" ] || fail "No se generó dist/lgpt_libretro.so"
sha256sum "$SRC/dist/lgpt_libretro.so" | tee "$SRC/dist/lgpt_libretro.so.u2_27.sha256"

echo "OK: U2.27 aplicado y compilado. Backup: $BACKUP_DIR"
