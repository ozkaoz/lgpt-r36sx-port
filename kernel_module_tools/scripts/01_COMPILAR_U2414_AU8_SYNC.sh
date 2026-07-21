#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/R36S/PORT LPTRACKER}"
TC="${TC:-$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot}"
WORK_ROOT="${WORK_ROOT:-$HOME/r36sx-kernel-u2414}"
SOURCE_PATH_FILE="$PROJECT_ROOT/KERNEL/U2414_KERNEL_SOURCE_PATH.txt"
OUT="$PROJECT_ROOT/BUILD/U2414"
LOGDIR="$PROJECT_ROOT/LOGS"
TS="$(date +%Y%m%d_%H%M%S)"
LOG="$LOGDIR/COMPILACION_U2414_AU8_SYNC_$TS.log"

mkdir -p "$OUT" "$LOGDIR"
exec > >(tee "$LOG") 2>&1

fail(){ echo "ERROR: $*" >&2; exit 1; }

for cmd in make rsync python3 file modinfo strings sha256sum bc; do
    command -v "$cmd" >/dev/null ||
        fail "Falta la dependencia de compilación: $cmd"
done

[[ -s "$SOURCE_PATH_FILE" ]] ||
    bash "$ROOT/scripts/00_PREPARAR_FUENTE_KERNEL_44186.sh"

KERNEL_SRC="$(cat "$SOURCE_PATH_FILE")"
[[ -f "$KERNEL_SRC/drivers/usb/gadget/function/f_uac2.c" ]] ||
    fail "Fuente del kernel inválida: $KERNEL_SRC"

CXX=""
for c in \
  "$TC/bin/mips-mti-linux-gnu-g++" \
  "$TC/opt/ext-toolchain/bin/mips-mti-linux-gnu-g++"; do
    [[ -x "$c" ]] && { CXX="$c"; break; }
done
if [[ -z "$CXX" ]]; then
    CXX="$(
        find "$TC" -maxdepth 6 -type f -perm -u+x \
            -name 'mips*-g++' -print -quit 2>/dev/null || true
    )"
fi
[[ -x "$CXX" ]] || fail "No se encontró el compilador MIPS."

CROSS="${CXX%g++}"
for tool in gcc g++ ar ld strip objcopy; do
    [[ -x "${CROSS}${tool}" ]] ||
        fail "Falta ${CROSS}${tool}"
done

if ! "$CXX" --version >/dev/null 2>&1; then
    [[ -x "$TC/relocate-sdk.sh" ]] ||
        fail "SDK no ejecutable y sin relocate-sdk.sh"
    (cd "$TC" && ./relocate-sdk.sh)
fi

echo "KERNEL_SRC=$KERNEL_SRC"
echo "CROSS_COMPILE=$CROSS"
echo "WORK_ROOT=$WORK_ROOT"
echo "OUT=$OUT"

rm -rf "$WORK_ROOT"
mkdir -p "$WORK_ROOT"
rsync -a --delete "$KERNEL_SRC/" "$WORK_ROOT/linux-4.4.186/"
BUILD_SRC="$WORK_ROOT/linux-4.4.186"
BUILD_OUT="$WORK_ROOT/build-rt305x-u2414"
mkdir -p "$BUILD_OUT"

PATCH_REPORT="$OUT/PATCH_SOURCE_U2414.json"
python3 "$ROOT/tools/patch_f_uac2_au8_sync.py" \
    "$BUILD_SRC/drivers/usb/gadget/function/f_uac2.c" \
    --backup "$OUT/f_uac2.c.before_u2414" \
    --report "$PATCH_REPORT"

make -C "$BUILD_SRC" \
    O="$BUILD_OUT" \
    ARCH=mips \
    CROSS_COMPILE="$CROSS" \
    rt305x_defconfig

CONFIG="$BUILD_OUT/.config"
[[ -s "$CONFIG" ]] || fail "No se generó .config"

python3 - "$CONFIG" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
lines = path.read_text(encoding="utf-8").splitlines()

required = {
    "CONFIG_LOCALVERSION": '"-release"',
    "CONFIG_LOCALVERSION_AUTO": "n",
    "CONFIG_SMP": "n",
    "CONFIG_PREEMPT": "y",
    "CONFIG_MODVERSIONS": "n",
    "CONFIG_MODULES": "y",
    "CONFIG_SOUND": "m",
    "CONFIG_SND": "m",
    "CONFIG_SND_PCM": "m",
    "CONFIG_USB_GADGET": "y",
    "CONFIG_USB_LIBCOMPOSITE": "m",
    "CONFIG_USB_CONFIGFS": "m",
    "CONFIG_USB_CONFIGFS_F_UAC2": "m",
}

def replace_symbol(lines, symbol, value):
    out = []
    found = False
    for line in lines:
        if line.startswith(symbol + "=") or line == f"# {symbol} is not set":
            if not found:
                if value == "n":
                    out.append(f"# {symbol} is not set")
                else:
                    out.append(f"{symbol}={value}")
                found = True
            continue
        out.append(line)
    if not found:
        if value == "n":
            out.append(f"# {symbol} is not set")
        else:
            out.append(f"{symbol}={value}")
    return out

for symbol, value in required.items():
    lines = replace_symbol(lines, symbol, value)

path.write_text("\n".join(lines) + "\n", encoding="utf-8")
PY

make -C "$BUILD_SRC" \
    O="$BUILD_OUT" \
    ARCH=mips \
    CROSS_COMPILE="$CROSS" \
    olddefconfig

python3 - "$CONFIG" <<'PYCONFIG'
from pathlib import Path
import sys

text = Path(sys.argv[1]).read_text(encoding="utf-8")
required = {
    'CONFIG_LOCALVERSION="-release"': True,
    '# CONFIG_LOCALVERSION_AUTO is not set': True,
    '# CONFIG_SMP is not set': True,
    'CONFIG_PREEMPT=y': True,
    '# CONFIG_MODVERSIONS is not set': True,
    'CONFIG_MODULES=y': True,
}
missing = [line for line in required if line not in text]
if missing:
    raise SystemExit("Required ABI config missing: " + ", ".join(missing))
if 'CONFIG_USB_CONFIGFS_F_UAC2=m' not in text and \
   'CONFIG_USB_CONFIGFS_F_UAC2=y' not in text:
    raise SystemExit('CONFIG_USB_CONFIGFS_F_UAC2 is not enabled.')
print('CONFIG_U2414_ABI_VERIFY_OK')
PYCONFIG

make -C "$BUILD_SRC" \
    O="$BUILD_OUT" \
    ARCH=mips \
    CROSS_COMPILE="$CROSS" \
    -j"$(nproc)" \
    modules_prepare

make -C "$BUILD_SRC" \
    O="$BUILD_OUT" \
    ARCH=mips \
    CROSS_COMPILE="$CROSS" \
    -j"$(nproc)" \
    M=drivers/usb/gadget/function \
    modules

MODULE="$BUILD_OUT/drivers/usb/gadget/function/usb_f_uac2.ko"
[[ -s "$MODULE" ]] || fail "No se generó usb_f_uac2.ko"

DESC="$(file -b "$MODULE")"
echo "FILE=$DESC"
[[ "$DESC" == *"ELF 32-bit"* &&
   "$DESC" == *"MIPS"* &&
   "$DESC" == *"relocatable"* ]] ||
    fail "Formato de módulo incorrecto."

VERMAGIC="$(modinfo -F vermagic "$MODULE" 2>/dev/null || true)"
echo "VERMAGIC=$VERMAGIC"
[[ "$VERMAGIC" == "4.4.186-release preempt MIPS32_R2 32BIT "* ||
   "$VERMAGIC" == "4.4.186-release preempt MIPS32_R2 32BIT" ]] ||
    fail "Vermagic incompatible: $VERMAGIC"

if modinfo "$MODULE" | grep -q 'modversions'; then
    fail "El módulo fue compilado con MODVERSIONS."
fi
if modinfo "$MODULE" | grep -q ' SMP '; then
    fail "El módulo fue compilado con SMP."
fi

strings "$MODULE" | grep -Fq 'R36SX_U2414_AU8_SYNC_REPLICA' ||
    fail "Falta el marcador U2.41.4."
strings "$MODULE" | grep -Fq 'R36SX USB AUDIO' ||
    fail "Falta el nombre R36SX USB AUDIO."

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
    raise SystemExit("Expected one Full-Speed synchronous OUT descriptor.")
if data.count(patterns["HS_OUT_SYNC"]) < 1:
    raise SystemExit("Expected at least one High-Speed synchronous OUT descriptor.")
if data.count(patterns["FS_IN_SYNC"]) != 1:
    raise SystemExit("Expected one Full-Speed synchronous IN descriptor.")

if bytes.fromhex("07 05 00 05 ff 03 01") in data:
    raise SystemExit("Asynchronous Full-Speed OUT descriptor remains.")
if bytes.fromhex("07 05 80 05 ff 03 01") in data:
    raise SystemExit("Asynchronous Full-Speed IN descriptor remains.")

print("AU8_SYNC_DESCRIPTOR_VERIFY_OK")
PY

install -m 0644 "$MODULE" "$OUT/usb_f_uac2_u2414_au8_sync.ko"
cp -f "$CONFIG" "$OUT/config_u2414_rt305x"
cp -f "$PATCH_REPORT" "$OUT/PATCH_SOURCE_U2414.json"

sha256sum \
    "$OUT/usb_f_uac2_u2414_au8_sync.ko" \
    "$OUT/config_u2414_rt305x" \
    "$OUT/PATCH_SOURCE_U2414.json" |
    tee "$OUT/SHA256SUMS.txt"

modinfo "$OUT/usb_f_uac2_u2414_au8_sync.ko" |
    tee "$OUT/MODINFO_U2414.txt"
file "$OUT/usb_f_uac2_u2414_au8_sync.ko" |
    tee "$OUT/FILE_U2414.txt"

echo "BUILD_U2414_AU8_SYNC_OK"
echo "MODULE=$OUT/usb_f_uac2_u2414_au8_sync.ko"
echo "LOG=$LOG"
