#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
for test_file in "$ROOT/tests/"test_*.py; do python3 "$test_file"; done
bash "$ROOT/tests/host_syntax_check.sh"
bash "$ROOT/tests/run_host_input_policy.sh"
bash "$ROOT/tests/run_host_navigation.sh"
bash "$ROOT/tests/run_host_help_overlay.sh"
bash "$ROOT/tests/run_host_chop_model.sh"
for f in "$ROOT/scripts/"*.sh; do bash -n "$f"; done
for f in otg_u241_common.sh otg_u241_setup_once.sh otg_u241_apply_profile_once.sh otg_u241_shutdown.sh; do sh -n "$ROOT/device/$f"; done
gcc -std=gnu99 -Wall -Wextra -Werror=implicit-function-declaration -fsyntax-only "$ROOT/device/r36s_u2523_usb_audio_io.c"
for forbidden in patches baselines prompts evidence; do [[ ! -e "$ROOT/$forbidden" ]]; done
[[ ! -e "$ROOT/source/tools" ]]
find "$ROOT" -type f \( -iname '*.ttf' -o -iname '*.otf' -o -iname '*.woff' -o -iname '*.woff2' \) -print -quit | grep -q . && { echo 'ERROR: font file found'; exit 1; } || true
echo AUDIT_CLEAN_MAIN_U2523_OK
