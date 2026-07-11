#!/usr/bin/env bash
set -euo pipefail

SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"
PATCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATCH_FILE="$PATCH_DIR/U2_26_CHOPPER_UI_PROGRESS_PREVIEW_FIX.patch"
CPP="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
HDR="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.h"
LIB="$SRC/sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp"
BACKUP_DIR="$SRC/backups/U2_26_CHOPPER_UI_PROGRESS_PREVIEW_FIX_$STAMP"
LOG="$SRC/BUILD_U2_26_CHOPPER_UI_PROGRESS_PREVIEW_FIX_$STAMP.log"

fail() { echo "ERROR: $*" >&2; exit 1; }

[ -d "$SRC" ] || fail "No existe SRC: $SRC"
[ -f "$CPP" ] || fail "No existe SampleChopperModal.cpp: $CPP"
[ -f "$HDR" ] || fail "No existe SampleChopperModal.h: $HDR"
[ -f "$LIB" ] || fail "No existe TreeFrogLibretro.cpp: $LIB"
[ -f "$PATCH_FILE" ] || fail "No existe patch: $PATCH_FILE"
[ -f "$SRC/BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh" ] || fail "No existe script de build estable en SRC"

grep -q "TREEFROG_U2_25_PITCH_ENV_SCOPE_UNDO_PROGRESS" "$CPP" || grep -q "TREEFROG_U2_26_CHOPPER_UI_PROGRESS_PREVIEW_FIX" "$CPP" || fail "U2.25 no parece aplicado; aplica/valida U2.25 antes de U2.26"
grep -q "void SampleChopperModal::previewPitchSetting" "$CPP" || fail "No encuentro previewPitchSetting; árbol inesperado"
grep -q "void SampleChopperModal::showOperationProgress" "$CPP" || fail "No encuentro showOperationProgress; árbol inesperado"
grep -q "retro_set_video_refresh" "$LIB" || fail "No encuentro retro_set_video_refresh; árbol inesperado"

mkdir -p "$BACKUP_DIR"
cp -p "$CPP" "$BACKUP_DIR/SampleChopperModal.cpp.before_u2_26"
cp -p "$HDR" "$BACKUP_DIR/SampleChopperModal.h.before_u2_26"
cp -p "$LIB" "$BACKUP_DIR/TreeFrogLibretro.cpp.before_u2_26"

if grep -q "TREEFROG_U2_26_CHOPPER_UI_PROGRESS_PREVIEW_FIX" "$CPP"; then
  echo "U2.26 ya parece aplicado; no reaplico patch. Continúo con build."
else
  cd "$SRC"
  patch --dry-run -p0 < "$PATCH_FILE" >/tmp/u2_26_patch_dryrun.log 2>&1 || {
    cat /tmp/u2_26_patch_dryrun.log >&2
    fail "El patch U2.26 no aplica limpio. Backup: $BACKUP_DIR"
  }
  patch -p0 < "$PATCH_FILE"
fi

grep -q "TREEFROG_U2_26_CHOPPER_UI_PROGRESS_PREVIEW_FIX" "$CPP" || fail "No quedó marcador U2.26"
grep -q "PITCH / ENVELOPE U2.26" "$CPP" || fail "No quedó UI U2.26"
grep -q "Graphical Chopper U2.26" "$CPP" || fail "No quedó cabecera Graphical Chopper U2.26"
grep -q "TreeFrogForceVideoRefresh" "$CPP" || fail "No quedó llamada de refresco forzado"
grep -q "void TreeFrogForceVideoRefresh" "$LIB" || fail "No quedó función de refresco forzado"
grep -q "selectPitchTargetSample" "$CPP" || fail "No quedó selector de sample en Pitch/Env"
grep -q "WavFile::Open(sourcePath.GetPath" "$CPP" || fail "No quedó preview con WavFile writer"
grep -q "StartStreamingRangeAt(path, 0" "$CPP" || fail "No quedó preview por range streaming"

cd "$SRC"
set +e
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh 2>&1 | tee "$LOG"
BUILD_RC=${PIPESTATUS[0]}
set -e

echo "BUILD_RC=$BUILD_RC"
[ "$BUILD_RC" -eq 0 ] || fail "Falló compilación. Log: $LOG"
[ -f "$SRC/dist/lgpt_libretro.so" ] || fail "No se generó dist/lgpt_libretro.so"
sha256sum "$SRC/dist/lgpt_libretro.so" | tee "$SRC/dist/lgpt_libretro.so.u2_26.sha256"

echo "OK: U2.26 aplicado y compilado. Backup: $BACKUP_DIR"
