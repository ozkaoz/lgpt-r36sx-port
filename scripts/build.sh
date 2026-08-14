#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/R36S/PORT LPTRACKER}"
SOURCE="${SOURCE:-$PROJECT_ROOT/WORK/U2523_SOURCE}"
TC="${TC:-$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot}"
BUILD_ROOT="${BUILD_ROOT:-$HOME/lgpt-r36sx-u2523-build}"
OUT_DIR="$PROJECT_ROOT/BUILD/U2523"
LOG_DIR="$PROJECT_ROOT/LOGS"
TS="$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUT_DIR" "$LOG_DIR"
exec > >(tee "$LOG_DIR/BUILD_U2523_$TS.log") 2>&1
fail(){ echo "ERROR: $*" >&2; exit 1; }
[[ -d "$SOURCE/sources" ]] || PROJECT_ROOT="$PROJECT_ROOT" bash "$ROOT/scripts/prepare_source.sh"
CXX="$TC/bin/mips-mti-linux-gnu-g++"
[[ -x "$CXX" ]] || fail "Missing MIPS compiler: $CXX"
CROSS="${CXX%g++}"
SYSROOT="${SYSROOT:-$TC/mipsel-buildroot-linux-gnu/sysroot}"
rm -rf "$BUILD_ROOT" && mkdir -p "$BUILD_ROOT"
rsync -a --delete "$SOURCE/" "$BUILD_ROOT/"
find "$BUILD_ROOT" -type f -exec touch {} +
TC="$TC" TOOLCHAIN="$TC" CROSS_COMPILE="$CROSS" SYSROOT="$SYSROOT" \
  TREEFROG_INPUT_DEBUG=0 TREEFROG_EVENT_DEBUG_ALL=0 \
  bash "$BUILD_ROOT/BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh"
CORE_RAW="$BUILD_ROOT/dist/lgpt_libretro.so"
CORE="$OUT_DIR/lgpt_r36sx_u2523.so"
DAEMON="$OUT_DIR/r36s_u2523_usb_audio_io"
[[ -s "$CORE_RAW" ]] || fail "Core was not generated"
"${CROSS}gcc" -mips32r2 -march=mips32r2 -mtune=74kc -mdspr2 -mfp32 -mhard-float -EL \
  --sysroot="$SYSROOT" -std=gnu99 -O2 -static -Wall -Wextra \
  -o "$DAEMON" "$ROOT/device/r36s_u2523_usb_audio_io.c"
install -m 0755 "$CORE_RAW" "$CORE"
chmod 0755 "$DAEMON"
for marker in U2523_RENAME_CARET_ALIGNMENT_GITHUB_FINAL U2520_PENDING_TAKE_RECORD_EDITOR_PHYSICAL_EDGE_READY R36SX_CAPTURE_ABI=2 LGPT_CRASHTRAP_ARMED_U2.53.0; do
  grep -aFq "$marker" "$CORE" || fail "Missing core marker: $marker"
done
grep -aFq 'R36SX_USB_AUDIO_DAEMON_ABI=7' "$DAEMON" || fail "Missing daemon ABI7 marker"
# Unified driver host-side backends: SP404MKII (host UAC2) and USB-MIDI.
"${CROSS}gcc" -mips32r2 -march=mips32r2 -mtune=74kc -mdspr2 -mfp32 -mhard-float -EL \
  --sysroot="$SYSROOT" -std=gnu99 -O2 -static -Wall -Wextra \
  -o "$OUT_DIR/r36s_sp404_host_audio_io" "$ROOT/device/r36s_sp404_host_audio_io.c" -lm
"${CROSS}gcc" -mips32r2 -march=mips32r2 -mtune=74kc -mdspr2 -mfp32 -mhard-float -EL \
  --sysroot="$SYSROOT" -std=gnu99 -O2 -static -Wall -Wextra \
  -o "$OUT_DIR/r36s_midi_host_io" "$ROOT/device/r36s_midi_host_io.c" -lm
chmod 0755 "$OUT_DIR/r36s_sp404_host_audio_io" "$OUT_DIR/r36s_midi_host_io"
grep -aFq 'R36SX_SP404_AUDIO_DAEMON_ABI=1' "$OUT_DIR/r36s_sp404_host_audio_io" || fail "Missing SP404 ABI1 marker"
grep -aFq 'R36SX_MIDI_DAEMON_ABI=1' "$OUT_DIR/r36s_midi_host_io" || fail "Missing MIDI ABI1 marker"
sha256sum "$CORE" "$DAEMON" "$OUT_DIR/r36s_sp404_host_audio_io" "$OUT_DIR/r36s_midi_host_io" | tee "$OUT_DIR/SHA256SUMS.txt"

# U2.52.6: strict diagnostic gate. Only GCC's real diagnostic format
# (file:line:col: error:/warning:) is accepted. Plain grep -i 'error' would
# match filenames like tinyxmlerror.cpp or paths like System/Errors/ and
# report false positives, so the scan keys on the exact compiler pattern.
DIAG_LOG="$LOG_DIR/BUILD_U2523_$TS.log"
DIAG_LINES="$(grep -aE ':[0-9]+:[0-9]+: (error|warning|note):' "$DIAG_LOG" || true)"
if [[ -n "$DIAG_LINES" ]]; then
    echo "DIAGNOSTIC_GATE_FAILED"
    printf '%s\n' "$DIAG_LINES"
    exit 1
fi
echo "DIAGNOSTIC_GATE=0_ERRORS_0_WARNINGS"
echo BUILD_U2523_OK
