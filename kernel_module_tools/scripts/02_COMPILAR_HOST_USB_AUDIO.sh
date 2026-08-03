#!/usr/bin/env bash
# Build the host-side USB audio/MIDI kernel modules for the unified driver:
#   snd-usb-audio.ko + snd-usbmidi-lib.ko  (sound/usb)
# These expose the SP404MKII as an ALSA sound card and USB-MIDI pianos as
# rawmidi nodes while musb-hdrc is in host role.
#
# Modeled on 01_COMPILAR_U2414_AU8_SYNC.sh; the running kernel already has
# USB=y / MUSB dual-role / SND*=m built, so only sound/usb modules are added.
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/R36S/PORT LPTRACKER}"
TC="${TC:-$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot}"
WORK_ROOT="${WORK_ROOT:-$HOME/r36sx-kernel-hostaudio}"
SOURCE_PATH_FILE="$PROJECT_ROOT/KERNEL/U2534_KERNEL_SOURCE_PATH.txt"
OUT="$PROJECT_ROOT/BUILD/HOST_USB_AUDIO"
LOGDIR="$PROJECT_ROOT/LOGS"
TS="$(date +%Y%m%d_%H%M%S)"
LOG="$LOGDIR/COMPILACION_HOST_USB_AUDIO_$TS.log"

mkdir -p "$OUT" "$LOGDIR"
exec > >(tee "$LOG") 2>&1

fail(){ echo "ERROR: $*" >&2; exit 1; }

for cmd in make rsync python3 file modinfo strings sha256sum bc; do
    command -v "$cmd" >/dev/null ||
        fail "Falta la dependencia de compilación: $cmd"
done

[[ -s "$SOURCE_PATH_FILE" ]] ||
    fail "Falta $SOURCE_PATH_FILE (ejecuta 00_PREPARAR_FUENTE_KERNEL_44186.sh)"

KERNEL_SRC="$(cat "$SOURCE_PATH_FILE")"
[[ -f "$KERNEL_SRC/sound/usb/card.c" ]] ||
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
    [[ -x "${CROSS}${tool}" ]] || fail "Falta ${CROSS}${tool}"
done
if ! "$CXX" --version >/dev/null 2>&1; then
    [[ -x "$TC/relocate-sdk.sh" ]] || fail "SDK no ejecutable"
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
BUILD_OUT="$WORK_ROOT/build-rt305x-hostaudio"
mkdir -p "$BUILD_OUT"

make -C "$BUILD_SRC" \
    O="$BUILD_OUT" \
    ARCH=mips \
    CROSS_COMPILE="$CROSS" HOSTCFLAGS=-fcommon \
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
    "CONFIG_MODULE_UNLOAD": "n",
    "CONFIG_MODULES": "y",
    "CONFIG_SOUND": "m",
    "CONFIG_SND": "m",
    "CONFIG_SND_TIMER": "m",
    "CONFIG_SND_PCM": "m",
    "CONFIG_SND_HWDEP": "m",
    "CONFIG_SND_RAWMIDI": "m",
    "CONFIG_SND_SEQUENCER": "m",
    "CONFIG_SND_RAWMIDI_SEQ": "m",
    "CONFIG_SND_USB": "y",
    "CONFIG_SND_USB_AUDIO": "m",
    "CONFIG_USB": "y",
    "CONFIG_USB_SUPPORT": "y",
    "CONFIG_USB_COMMON": "y",
    "CONFIG_USB_MUSB_HDRC": "y",
    "CONFIG_USB_MUSB_DUAL_ROLE": "y",
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
    CROSS_COMPILE="$CROSS" HOSTCFLAGS=-fcommon \
    olddefconfig

python3 - "$CONFIG" <<'PYCONFIG'
from pathlib import Path
import sys
text = Path(sys.argv[1]).read_text(encoding="utf-8")
required = {
    'CONFIG_SND_USB_AUDIO=m': True,
    'CONFIG_USB=y': True,
}
missing = [line for line in required if line not in text]
if missing:
    raise SystemExit("Required host-audio config missing: " + ", ".join(missing))
print('CONFIG_HOST_AUDIO_ABI_VERIFY_OK')
PYCONFIG

make -C "$BUILD_SRC" \
    O="$BUILD_OUT" \
    ARCH=mips \
    CROSS_COMPILE="$CROSS" HOSTCFLAGS=-fcommon \
    -j"$(nproc)" \
    modules_prepare

make -C "$BUILD_SRC" \
    O="$BUILD_OUT" \
    ARCH=mips \
    CROSS_COMPILE="$CROSS" HOSTCFLAGS=-fcommon \
    -j"$(nproc)" \
    M=sound/usb \
    modules

USB_AUDIO="$BUILD_OUT/sound/usb/snd-usb-audio.ko"
USB_MIDI_LIB="$BUILD_OUT/sound/usb/snd-usbmidi-lib.ko"
[[ -s "$USB_AUDIO" ]] || fail "No se generó snd-usb-audio.ko"
[[ -s "$USB_MIDI_LIB" ]] || fail "No se generó snd-usbmidi-lib.ko"

for MODULE in "$USB_AUDIO" "$USB_MIDI_LIB"; do
    DESC="$(file -b "$MODULE")"
    echo "FILE=$DESC"
    [[ "$DESC" == *"ELF 32-bit"* &&
       "$DESC" == *"MIPS"* &&
       "$DESC" == *"relocatable"* ]] ||
        fail "Formato de módulo incorrecto: $MODULE"
    VERMAGIC="$(modinfo -F vermagic "$MODULE" 2>/dev/null || true)"
    echo "VERMAGIC=$VERMAGIC"
    [[ "$VERMAGIC" == "4.4.186-release preempt MIPS32_R2 32BIT "* ||
       "$VERMAGIC" == "4.4.186-release preempt MIPS32_R2 32BIT" ]] ||
        fail "Vermagic incompatible: $MODULE ($VERMAGIC)"
done

install -m 0644 "$USB_AUDIO" "$OUT/snd-usb-audio.ko"
install -m 0644 "$USB_MIDI_LIB" "$OUT/snd-usbmidi-lib.ko"
cp -f "$CONFIG" "$OUT/config_host_usb_audio_rt305x"

sha256sum \
    "$OUT/snd-usb-audio.ko" \
    "$OUT/snd-usbmidi-lib.ko" \
    "$OUT/config_host_usb_audio_rt305x" |
    tee "$OUT/SHA256SUMS.txt"

modinfo "$OUT/snd-usb-audio.ko" | tee "$OUT/MODINFO_SND_USB_AUDIO.txt"
modinfo "$OUT/snd-usbmidi-lib.ko" | tee "$OUT/MODINFO_SND_USBMIDI_LIB.txt"
file "$OUT/snd-usb-audio.ko" | tee "$OUT/FILE_SND_USB_AUDIO.txt"

echo "BUILD_HOST_USB_AUDIO_OK"
echo "MODULES=$OUT/snd-usb-audio.ko $OUT/snd-usbmidi-lib.ko"
echo "LOG=$LOG"
n