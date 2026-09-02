#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/Toolchains/R36SX}"
TC="${TC:-$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot}"
WORK_ROOT="${WORK_ROOT:-$HOME/r36sx-kernel-48k-stereo}"
OUT="$PROJECT_ROOT/BUILD/U2414_48K_STEREO"
LOGDIR="$PROJECT_ROOT/LOGS"
TS="$(date +%Y%m%d_%H%M%S)"
LOG="$LOGDIR/COMPILACION_U2414_48K_STEREO_$TS.log"

mkdir -p "$OUT" "$LOGDIR"
exec > >(tee "$LOG") 2>&1

fail(){ echo "ERROR: $*" >&2; exit 1; }

for cmd in make rsync python3 file modinfo strings sha256sum bc; do
    command -v "$cmd" >/dev/null ||
        fail "Falta la dependencia de compilación: $cmd"
done

# GCC 10+ host defaults to -fno-common; kernel 4.4's scripts/dtc (bison)
# then fails with "multiple definition of yylloc". Keep -fcommon for host
# tools (same fix documented in README_CURRENT_MODULE.md for build 02).
export HOSTCFLAGS="-fcommon"

SOURCE_FILE="$PROJECT_ROOT/KERNEL/U2414_KERNEL_SOURCE_PATH.txt"
KERNEL_SRC=""
if [[ -s "$SOURCE_FILE" ]]; then
    KERNEL_SRC="$(cat "$SOURCE_FILE")"
    [[ -f "$KERNEL_SRC/drivers/usb/gadget/function/f_uac2.c" ]] || KERNEL_SRC=""
fi
if [[ -z "$KERNEL_SRC" ]]; then
    KERNEL_SRC="$PROJECT_ROOT/KERNEL/source/linux-4.4.186"
    [[ -f "$KERNEL_SRC/drivers/usb/gadget/function/f_uac2.c" ]] ||
        fail "No hay fuente del kernel (busca $KERNEL_SRC)."
fi

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
BUILD_OUT="$WORK_ROOT/build-rt305x-48kstereo"
mkdir -p "$BUILD_OUT"

PATCH_REPORT="$OUT/PATCH_SOURCE_U2_48K_STEREO.json"
python3 "$ROOT/tools/patch_f_uac2_48k_stereo.py" \
    "$BUILD_SRC/drivers/usb/gadget/function/f_uac2.c" \
    --backup "$OUT/f_uac2.c.before_48k_stereo" \
    --report "$PATCH_REPORT"

make -C "$BUILD_SRC" \
    O="$BUILD_OUT" \
    ARCH=mips \
    CROSS_COMPILE="$CROSS" \
    rt305x_defconfig

CONFIG="$BUILD_OUT/.config"
[[ -s "$CONFIG" ]] || fail "No se generó .config"

# Base proven config: the recovery config of the working AU8-SYNC build
# (same SoC/kernel/ABI), then re-run olddefconfig so Kconfig normalizes it.
RECOVERY_CFG="$ROOT/../recovery/u2_38au8_sync_uac2/config_u2414_rt305x"
if [[ -f "$RECOVERY_CFG" ]]; then
    echo "Config base: recovery/config_u2414_rt305x (build AU8 funcional)"
    cp -f "$RECOVERY_CFG" "$CONFIG"
fi

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
    'CONFIG_PREEMPT=y': True,
    '# CONFIG_MODVERSIONS is not set': True,
    'CONFIG_MODULES=y': True,
    'CONFIG_USB_CONFIGFS_F_UAC2=y': True,
    'CONFIG_USB_F_UAC2=m': True,
    'CONFIG_USB_CONFIGFS=m': True,
}
missing = [line for line in required if line not in text]
if missing:
    raise SystemExit("Required ABI config missing: " + ", ".join(missing))
print('CONFIG_U2_48K_STEREO_ABI_VERIFY_OK')
PYCONFIG

make -C "$BUILD_SRC" \
    O="$BUILD_OUT" \
    ARCH=mips \
    CROSS_COMPILE="$CROSS" \
    HOSTCFLAGS=-fcommon \
    -j"$(nproc)" \
    modules_prepare

make -C "$BUILD_SRC" \
    O="$BUILD_OUT" \
    ARCH=mips \
    CROSS_COMPILE="$CROSS" \
    HOSTCFLAGS=-fcommon \
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

strings "$MODULE" | grep -Fq 'R36SX_U2_48K_STEREO_2026' ||
    fail "Falta el marcador U2-48K-STEREO."
strings "$MODULE" | grep -Fq 'R36SX USB AUDIO' ||
    fail "Falta el nombre R36SX USB AUDIO."

DUMP="$OUT/DESCRIPTORES_U2_48K_STEREO.txt"
python3 "$ROOT/tools/dump_uac2_descriptors.py" "$MODULE" > "$DUMP" 2>&1 || true

SYNC_COUNT="$(grep -c 'sync=SYNC' "$DUMP" || true)"
ASYNC_COUNT="$(grep -c 'sync=ASYNC' "$DUMP" || true)"
echo "DESCRIPTOR_SYNC_ENDPOINTS=$SYNC_COUNT"
echo "DESCRIPTOR_ASYNC_ENDPOINTS=$ASYNC_COUNT"
[[ "$SYNC_COUNT" -ge 4 ]] || fail "No hay 4 endpoints SYNC en el bundle."
[[ "$ASYNC_COUNT" -eq 0 ]] || fail "Quedan endpoints ASYNC en el bundle."
grep -q 'CS CLOCK_SRC clk_id=' "$DUMP" || fail "No hay clock sources en el bundle."

install -m 0644 "$MODULE" "$OUT/usb_f_uac2_u2_48k_stereo.ko"
cp -f "$CONFIG" "$OUT/config_u2_48k_stereo_rt305x"
if [[ "$PATCH_REPORT" != "$OUT/PATCH_SOURCE_U2_48K_STEREO.json" ]]; then
    cp -f "$PATCH_REPORT" "$OUT/PATCH_SOURCE_U2_48K_STEREO.json"
fi

sha256sum \
    "$OUT/usb_f_uac2_u2_48k_stereo.ko" \
    "$OUT/config_u2_48k_stereo_rt305x" |
    tee "$OUT/SHA256SUMS.txt"

modinfo "$OUT/usb_f_uac2_u2_48k_stereo.ko" |
    tee "$OUT/MODINFO_U2_48K_STEREO.txt"

echo "BUILD_U2_48K_STEREO_OK"
echo "MODULE=$OUT/usb_f_uac2_u2_48k_stereo.ko"
echo "LOG=$LOG"
