#!/usr/bin/env bash
set -euo pipefail
SD_DRIVE="${1:-F}"; SD_DRIVE="${SD_DRIVE%:}"
OUT_BASE="${2:-/mnt/d/R36S/PORT LPTRACKER}"
SD="/mnt/${SD_DRIVE,,}"
STAMP="$(date +%Y%m%d_%H%M%S)"
OUT="$OUT_BASE/OTG_INVENTORY_AU11Z9_$STAMP"
ZIP="$OUT_BASE/OTG_INVENTORY_AU11Z9_$STAMP.zip"
fail(){ echo "ERROR: $*" >&2; exit 1; }
[ -d "$SD" ] || fail "SD not mounted: $SD"
mkdir -p "$OUT"
{
  echo "LGPT/R36SX OTG inventory AU11Z9"
  echo "date=$(date -Iseconds)"
  echo "sd=$SD"
  echo
  echo "[SD root selected folders]"
  for d in lgpt lgpt/otg cubegm cubegm/cores frogui roms roms/lgpt; do
    if [ -e "$SD/$d" ]; then echo "OK $d"; else echo "MISSING $d"; fi
  done
  echo
  echo "[LGPT OTG files]"
  find "$SD/lgpt/otg" -maxdepth 6 -type f 2>/dev/null | sed "s#$SD/##" | sort || true
  echo
  echo "[Kernel modules on SD under lgpt/otg]"
  find "$SD/lgpt/otg" -type f -name '*.ko' 2>/dev/null | sed "s#$SD/##" | sort || true
  echo
  echo "[Expected USB audio module names]"
  for n in soundcore.ko snd.ko snd-timer.ko snd-pcm.ko libcomposite.ko usb_f_uac2.ko usb_f_uac1.ko u_audio.ko g_audio.ko; do
    hits=$(find "$SD/lgpt/otg" -type f -name "$n" 2>/dev/null | wc -l | tr -d ' ')
    echo "$n=$hits"
  done
  echo
  echo "[Expected storage/MTP module names]"
  for n in usb_f_mtp.ko usb_f_ptp.ko usb_f_mass_storage.ko g_mass_storage.ko f_ium.ko ium.ko; do
    hits=$(find "$SD/lgpt/otg" -type f -name "$n" 2>/dev/null | wc -l | tr -d ' ')
    echo "$n=$hits"
  done
} > "$OUT/OTG_INVENTORY_AU11Z9.txt"

# Copy small runtime OTG scripts/log markers, but avoid copying big samples/projects.
mkdir -p "$OUT/lgpt_otg_listing"
find "$SD/lgpt/otg" -maxdepth 4 -type f 2>/dev/null | while read -r f; do
  case "$f" in
    *.ko) continue ;;
    *) rel="${f#$SD/}"; mkdir -p "$OUT/lgpt_otg_listing/$(dirname "$rel")"; cp -f --no-preserve=all "$f" "$OUT/lgpt_otg_listing/$rel" 2>/dev/null || cp -f "$f" "$OUT/lgpt_otg_listing/$rel" || true ;;
  esac
done

if command -v zip >/dev/null 2>&1; then
  ( cd "$OUT_BASE" && zip -qr "$ZIP" "$(basename "$OUT")" )
else
  python3 - <<PY
import zipfile, os
out=r'''$ZIP'''; base=r'''$OUT'''; root=os.path.dirname(base)
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
    for dp,dn,fn in os.walk(base):
        for f in fn:
            p=os.path.join(dp,f); z.write(p, os.path.relpath(p, root))
PY
fi

echo "SUMMARY=PASS_AU11Z9_OTG_INVENTORY_CREATED"
echo "OUT_DIR=$OUT"
echo "OUT_ZIP=$ZIP"
cat "$OUT/OTG_INVENTORY_AU11Z9.txt"
