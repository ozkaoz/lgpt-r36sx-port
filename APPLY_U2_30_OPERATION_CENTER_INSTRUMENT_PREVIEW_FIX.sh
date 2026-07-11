#!/usr/bin/env bash
set -euo pipefail

SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"
PATCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATCH_FILE="$PATCH_DIR/U2_30_OPERATION_CENTER_INSTRUMENT_PREVIEW_FIX.patch"
CPP="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
INST="$SRC/sources/Application/Views/InstrumentView.cpp"
BACKUP_DIR="$SRC/backups/U2_30_OPERATION_CENTER_INSTRUMENT_PREVIEW_FIX_$STAMP"
LOG="$SRC/BUILD_U2_30_OPERATION_CENTER_INSTRUMENT_PREVIEW_FIX_$STAMP.log"

fail() { echo "ERROR: $*" >&2; exit 1; }

[ -d "$SRC" ] || fail "No existe SRC: $SRC"
[ -f "$CPP" ] || fail "No existe SampleChopperModal.cpp: $CPP"
[ -f "$INST" ] || fail "No existe InstrumentView.cpp: $INST"
[ -f "$PATCH_FILE" ] || fail "No existe patch: $PATCH_FILE"
[ -f "$SRC/BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh" ] || fail "No existe script de build estable en SRC"

grep -q "TREEFROG_U2_29_CHOPPER_PANEL2_INSTRUMENT_PREVIEW_FIX\|TREEFROG_U2_30_OPERATION_CENTER_INSTRUMENT_PREVIEW_FIX" "$CPP" || fail "U2.29 no parece aplicado; valida U2.29 antes de U2.30"
grep -q "PITCH/ENV U2.29\|PITCH/ENV U2.30" "$CPP" || fail "No encuentro UI Pitch/Env esperada; árbol inesperado"
grep -q "previewCurrentSampleInstrument" "$INST" || fail "No encuentro helper de preescucha en InstrumentView"

mkdir -p "$BACKUP_DIR"
cp -p "$CPP" "$BACKUP_DIR/SampleChopperModal.cpp.before_u2_30"
cp -p "$INST" "$BACKUP_DIR/InstrumentView.cpp.before_u2_30"

if grep -q "TREEFROG_U2_30_OPERATION_CENTER_INSTRUMENT_PREVIEW_FIX" "$CPP"; then
  echo "U2.30 ya parece aplicado; no reaplico patch. Continúo con build."
else
  cd "$SRC"
  patch --dry-run -p0 < "$PATCH_FILE" >/tmp/u2_30_patch_dryrun.log 2>&1 || {
    cat /tmp/u2_30_patch_dryrun.log >&2
    fail "El patch U2.30 no aplica limpio. Backup: $BACKUP_DIR"
  }
  patch -p0 < "$PATCH_FILE"
fi

grep -q "TREEFROG_U2_30_OPERATION_CENTER_INSTRUMENT_PREVIEW_FIX" "$CPP" || fail "No quedó marcador U2.30 en Chopper"
grep -q "Graphical Chopper U2.30" "$CPP" || fail "No quedó cabecera Graphical Chopper U2.30"
grep -q "PITCH/ENV U2.30" "$CPP" || fail "No quedó panel PITCH/ENV U2.30"
grep -q "Press A to continue" "$CPP" || fail "No quedó pantalla Press A"
grep -q "TREEFROG_U2_30_INSTRUMENT_B_PREVIEW_STREAM_FIX" "$INST" || fail "No quedó fix U2.30 de Instrument preview"
grep -q "WavFile::Open(path.GetPath().c_str())" "$INST" || fail "No quedó apertura física WAV para preescucha Instrument"
grep -q "StartStreaming(path)" "$INST" || fail "No quedó fallback StartStreaming en Instrument"
grep -q "EPBM_L2" "$INST" || fail "No quedó stop L2+B en Instrument"

echo "== Grep de verificación U2.30 =="
grep -n "TREEFROG_U2_30\|Graphical Chopper U2.30\|PITCH/ENV U2.30\|TREEFROG_U2_30_INSTRUMENT\|WavFile::Open\|StartStreaming(path)" "$CPP" "$INST" || true

cd "$SRC"
set +e
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh 2>&1 | tee "$LOG"
BUILD_RC=${PIPESTATUS[0]}
set -e

echo "BUILD_RC=$BUILD_RC"
[ "$BUILD_RC" -eq 0 ] || fail "Falló compilación. Log: $LOG"
[ -f "$SRC/dist/lgpt_libretro.so" ] || fail "No se generó dist/lgpt_libretro.so"
sha256sum "$SRC/dist/lgpt_libretro.so" | tee "$SRC/dist/lgpt_libretro.so.u2_30.sha256"

echo "OK: U2.30 aplicado y compilado. Backup: $BACKUP_DIR"
