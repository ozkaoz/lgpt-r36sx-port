#!/bin/sh
set -u
LOGDIR=/mnt/sdcard/lgpt/otg/logs
mkdir -p "$LOGDIR" 2>/dev/null || true
LOG="$LOGDIR/u2_38au11_apply_profile_once.log"
if ! { : >>"$LOG"; } 2>/dev/null; then LOG=/tmp/u2_38au11_apply_profile_once.log; fi
exec >>"$LOG" 2>&1
COMMON=/mnt/sdcard/lgpt/otg/bin/otg_38au11_common.sh
MODE="${1:-}"
[ -f "$COMMON" ] || { echo ERROR_COMMON_MISSING="$COMMON"; exit 2; }
. "$COMMON"
echo "AU11V2_DIRECT_APPLY mode=$MODE policy=duplex_stable_always_open date=$(date)"
PROFILE=duplex_stable_always_open
CURRENT="$(cat /tmp/r36sx_au11_active_profile 2>/dev/null || true)"
echo "AU11V2_PROFILE_REQUEST mode=$MODE profile=$PROFILE current=$CURRENT"
echo "$MODE" > /tmp/r36sx_au11_last_direct_mode 2>/dev/null || true
load_audio_stack_au11 sync || true
if [ ! -d /sys/kernel/config/usb_gadget/r36sx_uac2_au11_duplex ] || [ "$CURRENT" != "$PROFILE" ]; then
  echo "AU11V2_RECREATE_GADGET_ONCE profile=$PROFILE from=$CURRENT"
  create_uac2_legacy_duplex_gadget_au11 musb-hdrc.0.auto "$PROFILE" || echo WARN_RECREATE_RC=$?
else
  echo AU11V2_KEEP_EXISTING_DUPLEX_CONTEXT_GATED="$PROFILE"
fi
echo "$PROFILE" > /tmp/r36sx_au11_active_profile 2>/dev/null || true
cat /tmp/r36sx_au11_active_profile > /mnt/sdcard/lgpt/otg/au11_active_usb_profile 2>/dev/null || true
for u in /sys/class/udc/*; do [ -e "$u/state" ] && echo UDC=$(basename "$u") STATE=$(cat "$u/state") SPEED=$(cat "$u/current_speed" 2>/dev/null); done
cat /proc/asound/cards || true
cat /proc/asound/pcm || true
exit 0
