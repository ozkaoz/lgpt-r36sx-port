#!/usr/bin/env bash
set -Eeuo pipefail

SD="${SD_MOUNT:-/mnt/f}"
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/Toolchains/R36SX}"
MODULE="$SD/lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2/usb_f_uac2.ko"
BUILD_MODULE="$PROJECT_ROOT/BUILD/U2414/usb_f_uac2_u2414_au8_sync.ko"
ERRORS=0

cf(){
    [[ -s "$1" ]] && echo "OK $1" ||
        { echo "ERROR $1"; ERRORS=$((ERRORS+1)); }
}
ce(){
    [[ -e "$1" ]] && echo "OK $1" ||
        { echo "ERROR $1"; ERRORS=$((ERRORS+1)); }
}

cf "$MODULE"
cf "$SD/lgpt/otg/bin/otg_u241_common.sh"
cf "$SD/lgpt/otg/bin/otg_u241_setup_once.sh"
cf "$SD/lgpt/otg/bin/otg_u241_apply_profile_once.sh"
cf "$SD/lgpt/otg/bin/otg_u241_shutdown.sh"
cf "$SD/lgpt/otg/bin/r36s_u241_usb_audio_io"
ce "$SD/lgpt/otg/enable_lgpt_uac2_bridge"
cf "$SD/LGPT_OTG_LOGS/U2414_AUDIO_DRIVER_README.txt"

DESC="$(file -b "$MODULE" 2>/dev/null || true)"
VERMAGIC="$(modinfo -F vermagic "$MODULE" 2>/dev/null || true)"
echo "MODULE_FILE=$DESC"
echo "MODULE_VERMAGIC=$VERMAGIC"

[[ "$DESC" == *"ELF 32-bit"* &&
   "$DESC" == *"MIPS"* &&
   "$DESC" == *"relocatable"* ]] ||
    ERRORS=$((ERRORS+1))

[[ "$VERMAGIC" == "4.4.186-release preempt MIPS32_R2 32BIT "* ||
   "$VERMAGIC" == "4.4.186-release preempt MIPS32_R2 32BIT" ]] ||
    ERRORS=$((ERRORS+1))

strings "$MODULE" | grep -Fq 'R36SX_U2414_AU8_SYNC_REPLICA' ||
    { echo "ERROR build marker"; ERRORS=$((ERRORS+1)); }
strings "$MODULE" | grep -Fq 'R36SX USB AUDIO' ||
    { echo "ERROR product string"; ERRORS=$((ERRORS+1)); }

python3 - "$MODULE" <<'PY'
from pathlib import Path
import sys

data = Path(sys.argv[1]).read_bytes()
patterns = {
    "FS_OUT_SYNC": bytes.fromhex("07 05 00 0d ff 03 01"),
    "HS_OUT_SYNC": bytes.fromhex("07 05 00 0d 00 04 04"),
    "FS_IN_SYNC": bytes.fromhex("07 05 80 0d ff 03 01"),
}
for name, pattern in patterns.items():
    print(f"{name}={data.count(pattern)}")

if data.count(patterns["FS_OUT_SYNC"]) != 1:
    raise SystemExit("FS OUT synchronous descriptor invalid.")
if data.count(patterns["HS_OUT_SYNC"]) < 1:
    raise SystemExit("HS OUT synchronous descriptor missing.")
if data.count(patterns["FS_IN_SYNC"]) != 1:
    raise SystemExit("FS IN synchronous descriptor invalid.")

if bytes.fromhex("07 05 00 05 ff 03 01") in data:
    raise SystemExit("Asynchronous FS OUT remains.")
if bytes.fromhex("07 05 80 05 ff 03 01") in data:
    raise SystemExit("Asynchronous FS IN remains.")

print("VERIFY_AU8_SYNC_DESCRIPTOR_OK")
PY

grep -Fq '0x38E8' "$SD/lgpt/otg/bin/otg_u241_common.sh" ||
    { echo "ERROR PID 38E8"; ERRORS=$((ERRORS+1)); }
grep -Fq 'R36SX-U2-38AU8-SYNC' "$SD/lgpt/otg/bin/otg_u241_common.sh" ||
    { echo "ERROR historical serial"; ERRORS=$((ERRORS+1)); }
grep -Fq 'R36SX USB AUDIO' "$SD/lgpt/otg/bin/otg_u241_common.sh" ||
    { echo "ERROR product name"; ERRORS=$((ERRORS+1)); }
grep -Fq 'u2_38au8_sync_uac2' "$SD/lgpt/otg/bin/otg_u241_common.sh" ||
    { echo "ERROR AU8 module path"; ERRORS=$((ERRORS+1)); }

if [[ -s "$BUILD_MODULE" ]]; then
    A="$(sha256sum "$BUILD_MODULE" | awk '{print $1}')"
    B="$(sha256sum "$MODULE" | awk '{print $1}')"
    echo "BUILD_SHA256=$A"
    echo "SD_SHA256=$B"
    [[ "$A" == "$B" ]] ||
        { echo "ERROR build/SD hash mismatch"; ERRORS=$((ERRORS+1)); }
fi

sync
echo "ERRORES=$ERRORS"
[[ "$ERRORS" -eq 0 ]] || exit 1
echo "VERIFY_U2414_AU8_SYNC_SD_OK"
