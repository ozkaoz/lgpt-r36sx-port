#!/usr/bin/env bash
set -euo pipefail
DRIVE="${1:-F}"; DRIVE="${DRIVE%:}"
SD="/mnt/${DRIVE,,}"
[ -d "$SD" ] || { echo "ERROR_SD_NOT_MOUNTED=$SD"; exit 4; }
echo "AU11Z6_SD_INVALID_ARGUMENT_DIAG=YES"
echo "SD=$SD"
echo "MOUNT_LINE=$(mount | grep -F " $SD " || true)"
echo "OTG_DIR=$SD/lgpt/otg"
mkdir -p "$SD/lgpt/otg" || true
ls -la "$SD/lgpt/otg" | sed 's/^/OTG_LS=/' || true
for f in enable_lgpt_uac2_bridge audio_driver_mode au11_usb_policy au11_active_usb_profile; do
  p="$SD/lgpt/otg/$f"
  echo "-- CHECK $p"
  [ -e "$p" ] && { ls -ld "$p" || true; stat "$p" || true; } || echo "ABSENT=$p"
  rm -f "$p" 2>/dev/null || echo "WARN_RM_FAILED=$p"
  tmp="$SD/lgpt/otg/.au11z6_test_$$.$f"
  if printf 'test\n' > "$tmp" 2>/dev/null; then
    echo "TMP_WRITE_OK=$tmp"
    rm -f "$tmp" || true
  else
    echo "TMP_WRITE_FAIL=$tmp"
  fi
done
sync || true
echo "SUMMARY=PASS_AU11Z6_SD_INVALID_ARGUMENT_DIAG_DONE"
