#!/usr/bin/env bash
set -euo pipefail
DRIVE="${1:-F}"; DRIVE="${DRIVE%:}"; SD="/mnt/${DRIVE,,}"
echo USB_OUT_AUTO_MUTE > "$SD/lgpt/otg/audio_driver_mode"
sync
echo "SUMMARY=PASS_SET_USB_OUT_AUTO_MUTE"
