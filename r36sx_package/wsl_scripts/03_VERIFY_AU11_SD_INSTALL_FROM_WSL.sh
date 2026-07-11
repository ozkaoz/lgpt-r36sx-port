#!/usr/bin/env bash
set -euo pipefail
DRIVE="${1:-F}"; DRIVE="${DRIVE%:}"
SD="/mnt/${DRIVE,,}"
[ -d "$SD" ] || { echo "ERROR SD mount not found: $SD"; exit 4; }
FAIL=0

need_file() {
  local p="$1"
  if [ ! -e "$p" ]; then echo "MISSING=$p"; FAIL=1; else echo "OK=$p"; fi
}

need_file "$SD/cubegm/cores/lgpt_libretro.so"
need_file "$SD/cubegm/lgpt_libretro.so"
need_file "$SD/lgpt/otg/bin/r36s_au11_usb_audio_io"
need_file "$SD/lgpt/otg/bin/otg_38au11_lgpt_sync_setup_once.sh"
need_file "$SD/lgpt/otg/bin/otg_38au11_apply_profile_once.sh"
need_file "$SD/lgpt/otg/bin/otg_38au11_common.sh"
need_file "$SD/lgpt/otg/U2_38AU11_INSTALL_INFO.txt"
need_file "$SD/lgpt/otg/audio_driver_mode"
need_file "$SD/lgpt/otg/au11_usb_policy"

INFO="$SD/lgpt/otg/U2_38AU11_INSTALL_INFO.txt"
POLICY="$SD/lgpt/otg/au11_usb_policy"
CORE="$SD/cubegm/cores/lgpt_libretro.so"
COMMON="$SD/lgpt/otg/bin/otg_38au11_common.sh"
SETUP="$SD/lgpt/otg/bin/otg_38au11_lgpt_sync_setup_once.sh"

if grep -qE 'U2_38AU11Z4_CAMILO_INSTALLED=YES|U2_38AU11Z5_CAMILO_INSTALLED=YES|U2_38AU11Z6_CAMILO_INSTALLED=YES|U2_38AU11Z_CAMILO_INSTALLED=YES' "$INFO" 2>/dev/null; then
  echo "OK_INSTALL_MARKER=AU11Z_COMPATIBLE"
else
  echo "ERROR install info lacks AU11Z/AU11Z4/AU11Z5 Camilo marker"
  FAIL=1
fi

if grep -qE 'DUPLEX_STABLE_ALWAYS_OPEN_AU11Z4_AU10Y_DESCRIPTOR|DUPLEX_STABLE_ALWAYS_OPEN_AU11Z5_AU10Y_DESCRIPTOR|DUPLEX_STABLE_ALWAYS_OPEN_AU11Z6_AU10Y_DESCRIPTOR|DUPLEX_STABLE_ALWAYS_OPEN_AU11Z_AU10Y_DESCRIPTOR' "$POLICY" 2>/dev/null; then
  echo "OK_USB_POLICY=AU11Z_COMPATIBLE_AU10Y_DESCRIPTOR"
else
  echo "ERROR USB policy is not AU11Z/AU11Z4/AU11Z5 duplex stable always-open"
  FAIL=1
fi

if ! grep -a -q 'U2_38AU11U_CLEANROOM_SD_WSL_WIN_PURGE_ROOT_DIAG' "$CORE"; then
  echo "ERROR core lacks AU11U cleanroom marker"
  FAIL=1
else
  echo "OK_CORE_MARKER=AU11U_CLEANROOM_STABLE_BASE"
fi

if strings -a "$CORE" | grep -q 'L2+A USB REC menu'; then
  echo "ERROR core still contains stale L2+A USB REC text"
  FAIL=1
else
  echo "OK_STALE_L2A_MENU_REMOVED=YES"
fi

if ! grep -qE 'AU11Z_AU10Y_DESCRIPTOR|AU11Z4_AU10Y_DESCRIPTOR|AU11Z5_AU10Y_DESCRIPTOR|AU11Z6_AU10Y_DESCRIPTOR' "$COMMON" 2>/dev/null; then
  echo "WARN common script lacks explicit AU11Z* descriptor marker; continuing if install policy is correct"
else
  echo "OK_COMMON_SCRIPT_MARKER=AU11Z_COMPATIBLE"
fi

if ! grep -qE 'U2.38AU11Z|U2.38AU11Z4|U2.38AU11Z5|U2.38AU11Z6' "$SETUP" 2>/dev/null; then
  echo "WARN setup script lacks explicit U2.38AU11Z* marker; continuing if required files exist"
else
  echo "OK_SETUP_SCRIPT_MARKER=AU11Z_COMPATIBLE"
fi

if grep -R 'U2_38AU11D_INSTALLED=YES\|SPLIT_USB_PROFILE_SAFE_AU11D' "$INFO" "$POLICY" 2>/dev/null; then
  echo "ERROR stale AU11D marker remains"
  FAIL=1
fi

if find "$SD/lgpt/otg/bin" -maxdepth 1 \( -name 'otg_38au10*.sh' -o -name 'r36s_au10*_usb_audio_io' -o -name 'r36s_au9*_fifo_to_uac2' \) | grep -q .; then
  echo "ERROR stale AU9/AU10 binaries/scripts remain:"
  find "$SD/lgpt/otg/bin" -maxdepth 1 \( -name 'otg_38au10*.sh' -o -name 'r36s_au10*_usb_audio_io' -o -name 'r36s_au9*_fifo_to_uac2' \)
  FAIL=1
else
  echo "OK_STALE_AU9_AU10_BINARIES_REMOVED=YES"
fi

if find "$SD/lgpt/otg/modules" -type f -name 'soundcore.ko' 2>/dev/null | grep -q .; then
  echo "OK_MODULES_PRESENT=YES"
else
  echo "WARN_MODULES_MISSING_USB_AUDIO_MAY_FAIL=YES"
fi

if [ -f "$INFO" ]; then
  echo "----- INSTALL INFO -----"
  cat "$INFO" || true
fi

[ "$FAIL" = 0 ] || { echo "SUMMARY=FAIL_AU11_SD_VERIFY"; exit 8; }
echo "SUMMARY=PASS_AU11_SD_VERIFY"
