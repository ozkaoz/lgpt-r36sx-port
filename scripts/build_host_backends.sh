#!/usr/bin/env bash
# Build the host-side backends (SP404 UAC2 + USB-MIDI) for the unified driver.
set -Eeuo pipefail
TC="${TC:-$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot}"
SYSROOT="${SYSROOT:-$TC/mipsel-buildroot-linux-gnu/sysroot}"
CC="$TC/bin/mips-mti-linux-gnu-gcc"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-/tmp/r36sx_daemon_test}"
mkdir -p "$OUT"
FAIL=0
for src in r36s_sp404_host_audio_io r36s_midi_host_io; do
  echo "BUILD $src"
  "$CC" -mips32r2 -march=mips32r2 -mtune=74kc -mdspr2 -mfp32 -mhard-float -EL \
    --sysroot="$SYSROOT" -std=gnu99 -O2 -static -Wall -Wextra \
    -o "$OUT/$src" "$ROOT/device/$src.c" || FAIL=1
done
if [ "$FAIL" -eq 0 ]; then
  echo "HOST_BACKENDS_BUILD_OK"
else
  echo "HOST_BACKENDS_BUILD_FAILED"
  exit 1
fi
