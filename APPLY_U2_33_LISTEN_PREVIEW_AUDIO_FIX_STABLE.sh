#!/usr/bin/env bash
set -euo pipefail

SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"
PATCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATCH_FILE="$PATCH_DIR/U2_33_LISTEN_PREVIEW_AUDIO_FIX_STABLE.patch"
CPP="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
INST="$SRC/sources/Application/Views/InstrumentView.cpp"
IMP="$SRC/sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp"
IMPH="$SRC/sources/Application/Views/ModalDialogs/ImportSampleDialog.h"
BACKUP_DIR="$SRC/backups/U2_33_LISTEN_PREVIEW_AUDIO_FIX_STABLE_$STAMP"
LOG="$SRC/BUILD_U2_33_LISTEN_PREVIEW_AUDIO_FIX_STABLE_$STAMP.log"

fail() { echo "ERROR: $*" >&2; exit 1; }

[ -d "$SRC" ] || fail "No existe SRC: $SRC"
[ -f "$CPP" ] || fail "No existe SampleChopperModal.cpp: $CPP"
[ -f "$INST" ] || fail "No existe InstrumentView.cpp: $INST"
[ -f "$IMP" ] || fail "No existe ImportSampleDialog.cpp: $IMP"
[ -f "$IMPH" ] || fail "No existe ImportSampleDialog.h: $IMPH"
[ -f "$PATCH_FILE" ] || fail "No existe patch: $PATCH_FILE"
[ -f "$SRC/BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh" ] || fail "No existe script de build estable en SRC"

grep -q "TREEFROG_U2_32_LISTEN_MENU_RESTORE_STABLE\|TREEFROG_U2_33_LISTEN_PREVIEW_AUDIO_FIX_STABLE" "$IMP" || fail "ImportSampleDialog no parece estar en U2.32/U2.33"
grep -q "Graphical Chopper U2.32\|Graphical Chopper U2.33" "$CPP" || fail "Chopper no parece estar en U2.32/U2.33"
grep -q "PITCH/ENV U2.32\|PITCH/ENV U2.33" "$CPP" || fail "Pitch/Env no parece estar en U2.32/U2.33"

mkdir -p "$BACKUP_DIR"
cp -p "$CPP" "$BACKUP_DIR/SampleChopperModal.cpp.before_u2_33"
cp -p "$INST" "$BACKUP_DIR/InstrumentView.cpp.before_u2_33"
cp -p "$IMP" "$BACKUP_DIR/ImportSampleDialog.cpp.before_u2_33"
cp -p "$IMPH" "$BACKUP_DIR/ImportSampleDialog.h.before_u2_33"

if grep -q "TREEFROG_U2_33_LISTEN_PREVIEW_AUDIO_FIX_STABLE" "$IMP" && \
   grep -q "StartStreamingRangeAt(path, 0, previewFrames" "$IMP" && \
   grep -q "PITCH/ENV U2.33" "$CPP"; then
  echo "U2.33 ya parece aplicado; no reaplico patch. Continúo con build."
else
  cd "$SRC"
  patch --dry-run -p0 < "$PATCH_FILE" >/tmp/u2_33_patch_dryrun.log 2>&1 || {
    cat /tmp/u2_33_patch_dryrun.log >&2
    fail "El patch U2.33 no aplica limpio. Backup: $BACKUP_DIR"
  }
  patch -p0 < "$PATCH_FILE"
fi

grep -q "TREEFROG_U2_33_LISTEN_PREVIEW_AUDIO_FIX_STABLE" "$CPP" || fail "No quedó marcador U2.33 en Chopper"
grep -q "TREEFROG_U2_33_LISTEN_PREVIEW_AUDIO_FIX_STABLE" "$IMP" || fail "No quedó marcador U2.33 en ImportSampleDialog"
grep -q "TREEFROG_U2_33_LISTEN_PREVIEW_AUDIO_FIX_STABLE" "$INST" || fail "No quedó marcador U2.33 en InstrumentView"
grep -q "Graphical Chopper U2.33" "$CPP" || fail "No quedó cabecera Graphical Chopper U2.33"
grep -q "PITCH/ENV U2.33" "$CPP" || fail "No quedó panel PITCH/ENV U2.33"
grep -q "buildListenPreviewWav(Path &element, std::string &logicalPath, int &frames)" "$IMP" || fail "No quedó helper Listen con frames"
grep -q "StartStreamingRangeAt(path, 0, previewFrames" "$IMP" || fail "Listen no quedó usando StartStreamingRangeAt"
if grep -q "Listen preview\|Listen direct" "$IMP"; then
  fail "Listen todavía dibuja mensaje de preview; U2.33 debe reproducir sin mensaje"
fi
if grep -q "previewCurrentSampleInstrument\|plainBPreview\|__u2_instrument_preview" "$INST"; then
  fail "InstrumentView volvió a contener preescucha directa con B"
fi

echo "== Grep de verificación U2.33 =="
grep -n "TREEFROG_U2_33\|Graphical Chopper U2.33\|PITCH/ENV U2.33\|StartStreamingRangeAt(path, 0, previewFrames\|__u2_listen_preview.wav" "$CPP" "$INST" "$IMP" "$IMPH" || true

cd "$SRC"
set +e
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh 2>&1 | tee "$LOG"
BUILD_RC=${PIPESTATUS[0]}
set -e

echo "BUILD_RC=$BUILD_RC"
[ "$BUILD_RC" -eq 0 ] || fail "Falló compilación. Log: $LOG"
[ -f "$SRC/dist/lgpt_libretro.so" ] || fail "No se generó dist/lgpt_libretro.so"
ls -lh "$SRC/dist/lgpt_libretro.so"
sha256sum "$SRC/dist/lgpt_libretro.so"
echo "OK: U2.33 aplicado y compilado. Backup: $BACKUP_DIR"
