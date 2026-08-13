#!/usr/bin/env bash
# run_host_help_overlay.sh -- F2: compila y ejecuta el harness host del
# HelpOverlay con los .cpp reales (View/ModalView/HelpOverlay/HelpRegistry/
# UiDraw/NavigationController) + stubs, bajo ASAN/UBSAN. Reproduce la
# secuencia R1-en-help que cae en consola.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="${TMPDIR:-/tmp}/help_overlay_host"
rm -rf "$TMP" && mkdir -p "$TMP"
g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -DPLATFORM_TREEFROG -DTREEFROG_INPUT_PROFILE=0 -DTREEFROG_AUDIO_MODE=0 \
  -DTREEFROG_TIMER_MODE=1 -DTREEFROG_VIDEO_MODE=0 \
  -DCPP_MEMORY -DHAVE_STDINT_H -D_NDEBUG -D_NO_JACK_ -DDUMMYMIDI \
  -D_LGPT_NO_SCREEN_CACHE_ -include stdint.h \
  -include "$SRC/Adapters/TREEFROG/Compat/SDL_types_force.h" \
  -I"$SRC/Adapters/TREEFROG/Compat" \
  -I"$SRC" \
  "$SRC/Application/Views/BaseClasses/View.cpp" \
  "$SRC/Application/Views/BaseClasses/ModalView.cpp" \
  "$SRC/Application/Views/BaseClasses/HelpOverlay.cpp" \
  "$SRC/Application/Views/BaseClasses/HelpRegistry.cpp" \
  "$SRC/Application/Views/BaseClasses/UiDraw.cpp" \
  "$SRC/Application/UI/Navigation/NavigationController.cpp" \
  "$ROOT/tests/host/help_overlay_host_stubs.cpp" \
  "$ROOT/tests/host/help_overlay_host_test.cpp" \
  -o "$TMP/help_overlay_host_test"
"$TMP/help_overlay_host_test"