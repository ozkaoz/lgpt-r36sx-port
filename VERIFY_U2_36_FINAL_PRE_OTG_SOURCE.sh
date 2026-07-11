#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
fail(){ echo "ERROR: $*" >&2; exit 1; }
check_file(){ [ -f "$ROOT/$1" ] || fail "Falta $1"; }
check_grep(){ grep -q "$1" "$ROOT/$2" || fail "No se encontró '$1' en $2"; }

check_file "sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
check_file "sources/Application/Views/ModalDialogs/SampleChopperModal.h"
check_file "sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp"
check_file "sources/Application/Views/ModalDialogs/SampleManagerDialog.cpp"
check_file "sources/Application/Views/ModalDialogs/SampleManagerDialog.h"
check_file "sources/Application/Instruments/SamplePool.cpp"
check_file "sources/Application/Instruments/SamplePool.h"
check_file "sources/Application/Views/InstrumentView.cpp"
check_file "sources/Application/Views/SongView.cpp"
check_file "projects/Makefile"
check_file "BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh"
check_file "installers/stock_treefrogui_clean/LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER/INSTALL_U2_36_STOCK_TREEFROGUI_FROM_WSL.sh"
check_file "installers/stock_treefrogui_clean/LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER/VERIFY_U2_36_STOCK_TREEFROGUI_FROM_WSL.sh"
check_file "README_FINAL_ESTABLE_PRE_OTG_U2_36_ES.md"
check_file "PROMPT_CONTINUAR_DESARROLLO_U2_37_OTG_AUDIO_ES.md"
check_file "OTG_AUDIO_STATUS_U2_37_PRE_PATCH_ES.md"

check_grep "Graphical Chopper U2.36" "sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
check_grep "PITCH/ENV U2.36" "sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
check_grep "TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE" "sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
check_grep "TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE" "sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp"
check_grep "TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE" "sources/Application/Views/ModalDialogs/SampleManagerDialog.cpp"
check_grep "TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE" "sources/Application/Instruments/SamplePool.cpp"
check_grep "FindIdenticalProjectSample" "sources/Application/Instruments/SamplePool.cpp"
check_grep "SampleManagerDialog.o" "projects/Makefile"
check_grep "LGPT U2.36 STOCK + TREEFROGUI CLEAN INSTALL" "installers/stock_treefrogui_clean/LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER/INSTALL_U2_36_STOCK_TREEFROGUI.ps1"

echo "OK: U2.36 FINAL PRE-OTG fuente/instaladores verificados."
