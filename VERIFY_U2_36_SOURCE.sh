#!/usr/bin/env bash
set -euo pipefail
SRC="${1:-$(pwd)}"
CPP="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
IMP="$SRC/sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp"
MGR="$SRC/sources/Application/Views/ModalDialogs/SampleManagerDialog.cpp"
POOL="$SRC/sources/Application/Instruments/SamplePool.cpp"
CORE="$SRC/dist/lgpt_libretro.so"

grep -n "TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE" "$CPP" "$IMP" "$MGR" "$POOL"
grep -n "Graphical Chopper U2.36" "$CPP"
grep -n "PITCH/ENV U2.36" "$CPP"
grep -n "FindIdenticalProjectSample" "$POOL"

if [ -f "$CORE" ]; then
  ls -lh "$CORE"
  file "$CORE"
  sha256sum "$CORE"
else
  echo "AVISO: core no compilado todavía: $CORE"
fi

echo "OK: fuente U2.36 verificada."
