#!/usr/bin/env bash
set -Eeuo pipefail
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/R36S/PORT LPTRACKER}"
SD="${SD_MOUNT:-/mnt/f}"
TS="$(date +%Y%m%d_%H%M%S)"
WORK="$PROJECT_ROOT/LOGS/LGPT_U2523_$TS"
ZIP="$WORK.zip"
mkdir -p "$WORK/runtime_state" "$WORK/records"
{
  echo "COLLECTED_AT=$(date -Is)"
  echo "PROFILE=$(cat "$SD/lgpt/otg/audio_usb_profile" 2>/dev/null || echo missing)"
  for f in "$SD/cubegm/cores/lgpt_core.so" "$SD/cubegm/cores/lgpt_libretro.so" "$SD/cubegm/cores/lgpt_r36sx_port_libretro.so" "$SD/lgpt/otg/bin/r36s_u241_usb_audio_io" "$SD/lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2/usb_f_uac2.ko"; do [[ -f "$f" ]] && { file "$f"; sha256sum "$f"; }; done
} > "$WORK/STATE.txt"
for f in "$SD/LGPT_OTG_LOGS/U2517_AUDIO_DRIVER_SETUP.log" "$SD/LGPT_OTG_LOGS/U2517_AUDIO_DRIVER_SNAPSHOT.txt" "$SD/LGPT_OTG_LOGS/U2517_USB_AUDIO_DAEMON.log" "$SD/LGPT_OTG_LOGS/uac2_bridge_lgpt.log" "$SD/lgpt/otg/logs/u241_setup_from_lgpt.log"; do [[ -f "$f" ]] && cp -f "$f" "$WORK/$(basename "$f")"; done
find "$SD/lgpt/otg/logs/runtime_state" -maxdepth 1 -type f -exec cp -f {} "$WORK/runtime_state/" \; 2>/dev/null || true
find "$SD/lgpt/samples/records" -maxdepth 1 -type f -iname '*.wav' -exec cp -f {} "$WORK/records/" \; 2>/dev/null || true
python3 - "$WORK" "$ZIP" <<'PY2'
from pathlib import Path
import sys,zipfile
root=Path(sys.argv[1]); out=Path(sys.argv[2])
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
    for p in root.rglob('*'):
        if p.is_file(): z.write(p,Path(root.name)/p.relative_to(root))
print(out)
PY2
echo COLLECT_LOGS_U2523_OK