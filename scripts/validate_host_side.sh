#!/usr/bin/env bash
set -Eeuo pipefail
FAIL=0
for s in device/otg_h37_apply_driver_mode.sh \
         device/otg_h37_android_runtime_supervisor.sh \
         device/otg_h37_host_runtime_supervisor.sh \
         device/otg_h37_host_device_detect.sh \
         device/otg_u241_apply_profile_once.sh; do
  if sh -n "$s"; then echo "SYNTAX_OK $s"; else echo "SYNTAX_FAIL $s"; FAIL=1; fi
done
bash scripts/build_host_backends.sh
exit "$FAIL"
