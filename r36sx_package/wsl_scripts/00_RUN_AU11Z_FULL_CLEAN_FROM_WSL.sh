#!/usr/bin/env bash
set -euo pipefail
WORKDIR="${1:-/mnt/d/R36S/PORT LPTRACKER}"
SD_DRIVE="${2:-F}"
BUILD_ROOT="${3:-/tmp/r36s_u2_38au11z4}"
WINDOWS_CLEAN_MODE="${4:-skip}"   # remove | list | skip. En AU11Z4 se prefiere limpiar Windows con PASO_0 elevado.
PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOGDIR="$BUILD_ROOT/U2_38AU11_BUILD_OUT"
mkdir -p "$LOGDIR"
on_error() {
  code=$?
  echo ""
  echo "SUMMARY=FAIL_AU11Z6_CAMILO_FULL_CLEAN_MARKERWRITEFIX"
  echo "EXIT_CODE=$code"
  echo "BUILD_ROOT=$BUILD_ROOT"
  echo "LOGDIR=$LOGDIR"
  for f in "$LOGDIR/windows_clean_au11z.log" "$LOGDIR/cleanroom_preinstall_au11z.log" "$LOGDIR/preflight_au11_modified_cpp.log" "$LOGDIR/build_daemon.log" "$LOGDIR/build_lgpt_au11.log" "$LOGDIR/U2_38AU11_VALIDATION_SUMMARY.txt" "$LOGDIR/install_au11z.log" "$LOGDIR/verify_au11z.log"; do
    [ -f "$f" ] && { echo "----- tail: $f -----"; tail -180 "$f" || true; }
  done
  exit "$code"
}
trap on_error ERR
cd "$PKG_DIR"
chmod +x bin/*.sh device/*.sh || true
echo "AU11Z4 Camilo Peña WSL24 SD copyfix cleanhost build/install"
echo "WORKDIR=$WORKDIR"
echo "PKG_DIR=$PKG_DIR"
echo "BUILD_ROOT=$BUILD_ROOT"
echo "SD_DRIVE=$SD_DRIVE"
echo "WINDOWS_CLEAN_MODE=$WINDOWS_CLEAN_MODE"
echo "COPY_POLICY=NO_PRESERVE_TIMES_MODES_OWNERSHIP_FOR_WSL_SD"

if [ "${WINDOWS_CLEAN_MODE,,}" != "skip" ]; then
  set +e
  bash bin/00_CLEAN_WINDOWS_R36SX_USB_DEVICES_FROM_WSL.sh "$WINDOWS_CLEAN_MODE" 2>&1 | tee "$LOGDIR/windows_clean_au11z.log"
  win_rc=${PIPESTATUS[0]}
  set -e
  echo "WINDOWS_CLEAN_RC=$win_rc" | tee -a "$LOGDIR/windows_clean_au11z.log"
  if [ "$win_rc" != 0 ]; then
    echo "WARN_WINDOWS_CLEAN_NOT_COMPLETE=YES"
    echo "NEXT_WINDOWS_CLEAN=Run windows/R36SX_AU11Z_WINDOWS_CLEAN_ADMIN.cmd as Administrator before USB test."
  fi
else
  echo "WINDOWS_CLEAN_SKIPPED=YES" | tee "$LOGDIR/windows_clean_au11z.log"
fi

bash bin/00_CLEANROOM_SD_WSL_PREINSTALL_AU11Z.sh "$SD_DRIVE" "$BUILD_ROOT" "$WORKDIR" 2>&1 | tee "$LOGDIR/cleanroom_preinstall_au11z.log"
bash bin/01_BUILD_LGPT_AU11.sh "$WORKDIR" "$BUILD_ROOT"
cat "$BUILD_ROOT/U2_38AU11_BUILD_OUT/U2_38AU11_VALIDATION_SUMMARY.txt"
bash bin/02_INSTALL_AU11_TO_SD_FROM_WSL.sh "$SD_DRIVE" "$BUILD_ROOT/U2_38AU11_BUILD_OUT" 2>&1 | tee "$LOGDIR/install_au11z.log"
bash bin/03_VERIFY_AU11_SD_INSTALL_FROM_WSL.sh "$SD_DRIVE" 2>&1 | tee "$LOGDIR/verify_au11z.log"
BUILD_CORE="$BUILD_ROOT/U2_38AU11_BUILD_OUT/lgpt_libretro_au11u_cleanroom_root_diag.so"
BUILD_DAEMON="$BUILD_ROOT/U2_38AU11_BUILD_OUT/r36s_au11_usb_audio_io"
SD="/mnt/${SD_DRIVE,,}"
if cmp -s "$BUILD_CORE" "$SD/cubegm/cores/lgpt_libretro.so"; then CORE_1_CMP=0; else CORE_1_CMP=1; fi
if cmp -s "$BUILD_CORE" "$SD/cubegm/lgpt_libretro.so"; then CORE_2_CMP=0; else CORE_2_CMP=1; fi
if cmp -s "$BUILD_DAEMON" "$SD/lgpt/otg/bin/r36s_au11_usb_audio_io"; then DAEMON_CMP=0; else DAEMON_CMP=1; fi
echo "CORE_1_CMP=$CORE_1_CMP"
echo "CORE_2_CMP=$CORE_2_CMP"
echo "DAEMON_CMP=$DAEMON_CMP"
if [ "$CORE_1_CMP$CORE_2_CMP$DAEMON_CMP" != "000" ]; then
  echo "SUMMARY=FAIL_AU11Z4_COMPARE_AFTER_INSTALL"
  exit 9
fi
sync
echo "SUMMARY=PASS_AU11Z6_CAMILO_FULL_CLEAN_COPYFIX_VERIFYFIX_MARKERWRITEFIX"
echo "NEXT=Do not test unless PASS_AU11_SD_VERIFY and compare values are 0. Then power off R36SX fully and test navigation before USB."
