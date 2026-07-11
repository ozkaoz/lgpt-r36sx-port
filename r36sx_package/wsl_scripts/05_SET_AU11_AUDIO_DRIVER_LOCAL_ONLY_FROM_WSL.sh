#!/usr/bin/env bash
set -euo pipefail
DRIVE="${1:-F}"; DRIVE="${DRIVE%:}"; SD="/mnt/${DRIVE,,}"
echo LOCAL_ONLY > "$SD/lgpt/otg/audio_driver_mode"
sync
echo "SUMMARY=PASS_SET_LOCAL_ONLY"
