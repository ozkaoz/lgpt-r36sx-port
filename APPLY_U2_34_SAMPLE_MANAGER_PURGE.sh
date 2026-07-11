#!/usr/bin/env bash
set -euo pipefail

SRC="${1:-/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT}"
STAMP="$(date +%Y%m%d_%H%M%S)"
PATCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATCH_FILE="$PATCH_DIR/U2_34_SAMPLE_MANAGER_PURGE.patch"
CPP="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
IMP="$SRC/sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp"
MGR="$SRC/sources/Application/Views/ModalDialogs/SampleManagerDialog.cpp"
MGRH="$SRC/sources/Application/Views/ModalDialogs/SampleManagerDialog.h"
MK="$SRC/projects/Makefile"
BACKUP_DIR="$SRC/backups/U2_34_SAMPLE_MANAGER_PURGE_$STAMP"
LOG="$SRC/BUILD_U2_34_SAMPLE_MANAGER_PURGE_$STAMP.log"

fail() { echo "ERROR: $*" >&2; exit 1; }

[ -d "$SRC" ] || fail "No existe SRC: $SRC"
[ -f "$CPP" ] || fail "No existe SampleChopperModal.cpp: $CPP"
[ -f "$IMP" ] || fail "No existe ImportSampleDialog.cpp: $IMP"
[ -f "$MK" ] || fail "No existe projects/Makefile: $MK"
[ -f "$PATCH_FILE" ] || fail "No existe patch: $PATCH_FILE"
[ -f "$SRC/BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh" ] || fail "No existe script de build estable en SRC"

grep -q "TREEFROG_U2_33_LISTEN_PREVIEW_AUDIO_FIX_STABLE\|TREEFROG_U2_34_SAMPLE_MANAGER_PURGE" "$IMP" || fail "ImportSampleDialog no parece estar en U2.33/U2.34"
grep -q "Graphical Chopper U2.33\|Graphical Chopper U2.34" "$CPP" || fail "Chopper no parece estar en U2.33/U2.34"
grep -q "PITCH/ENV U2.33\|PITCH/ENV U2.34" "$CPP" || fail "Pitch/Env no parece estar en U2.33/U2.34"

mkdir -p "$BACKUP_DIR"
cp -p "$CPP" "$BACKUP_DIR/SampleChopperModal.cpp.before_u2_34"
cp -p "$IMP" "$BACKUP_DIR/ImportSampleDialog.cpp.before_u2_34"
cp -p "$MK" "$BACKUP_DIR/Makefile.before_u2_34"
[ ! -f "$MGR" ] || cp -p "$MGR" "$BACKUP_DIR/SampleManagerDialog.cpp.before_u2_34"
[ ! -f "$MGRH" ] || cp -p "$MGRH" "$BACKUP_DIR/SampleManagerDialog.h.before_u2_34"

if grep -q "TREEFROG_U2_34_SAMPLE_MANAGER_PURGE" "$IMP" && \
   grep -q "SampleManagerDialog.o" "$MK" && \
   [ -f "$MGR" ] && [ -f "$MGRH" ] && \
   grep -q "Graphical Chopper U2.34" "$CPP"; then
  echo "U2.34 ya parece aplicado; no reaplico patch. Continúo con build."
else
  cd "$SRC"
  patch --dry-run -p0 < "$PATCH_FILE" >/tmp/u2_34_patch_dryrun.log 2>&1 || {
    cat /tmp/u2_34_patch_dryrun.log >&2
    fail "El patch U2.34 no aplica limpio. Backup: $BACKUP_DIR"
  }
  patch -p0 < "$PATCH_FILE"
fi

grep -q "TREEFROG_U2_34_SAMPLE_MANAGER_PURGE" "$CPP" || fail "No quedó marcador U2.34 en Chopper"
grep -q "TREEFROG_U2_34_SAMPLE_MANAGER_PURGE" "$IMP" || fail "No quedó marcador U2.34 en ImportSampleDialog"
grep -q "TREEFROG_U2_34_SAMPLE_MANAGER_PURGE" "$MGR" || fail "No quedó marcador U2.34 en SampleManagerDialog"
grep -q "Graphical Chopper U2.34" "$CPP" || fail "No quedó cabecera Graphical Chopper U2.34"
grep -q "PITCH/ENV U2.34" "$CPP" || fail "No quedó panel PITCH/ENV U2.34"
grep -q "SampleManagerDialog.o" "$MK" || fail "Makefile no compila SampleManagerDialog.o"
grep -q "LGPTChopperHasSavedChopsForSampleIndex" "$CPP" || fail "No quedó helper de chops para purge"
grep -q "LGPTChopperOnSamplePoolDelete" "$CPP" || fail "No quedó helper de ajuste de índices"
grep -q "PROJECT SAMPLE MANAGER" "$MGR" || fail "No quedó UI del Sample Manager"
grep -q "A del unused  Y purge" "$MGR" || fail "No quedó ayuda del Sample Manager"

echo "== Grep de verificación U2.34 =="
grep -n "TREEFROG_U2_34\|Graphical Chopper U2.34\|PITCH/ENV U2.34\|PROJECT SAMPLE MANAGER\|SampleManagerDialog.o" "$CPP" "$IMP" "$MGR" "$MGRH" "$MK" || true

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
echo "OK: U2.34 aplicado y compilado. Backup: $BACKUP_DIR"
