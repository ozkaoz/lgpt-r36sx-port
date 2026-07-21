#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/R36S/PORT LPTRACKER}"
SD="${SD_MOUNT:-/mnt/f}"
BUILD="$PROJECT_ROOT/BUILD/U2414"
MODULE="$BUILD/usb_f_uac2_u2414_au8_sync.ko"
CONFIG="$BUILD/config_u2414_rt305x"
PATCH_REPORT="$BUILD/PATCH_SOURCE_U2414.json"
BIN="$SD/lgpt/otg/bin"
MODROOT="$SD/lgpt/otg/modules/4.4.186-release"
AU8DIR="$MODROOT/u2_38au8_sync_uac2"
LOGROOT="$SD/LGPT_OTG_LOGS"
TS="$(date +%Y%m%d_%H%M%S)"
BACKUP="$PROJECT_ROOT/BACKUPS/LGPT_U2413_BEFORE_U2414_$TS"
LOGDIR="$PROJECT_ROOT/LOGS"
LOG="$LOGDIR/INSTALACION_U2414_AU8_SYNC_$TS.log"

mkdir -p "$BACKUP" "$LOGDIR"
exec > >(tee "$LOG") 2>&1

fail(){ echo "ERROR: $*" >&2; exit 1; }
run_sd(){ "$@" 2>/dev/null || sudo "$@"; }

echo "== INSTALL U2.41.4 AU8-SYNC REPLICA =="
date -Is
findmnt -T "$SD" || true
df -hT "$SD" || true

for cmd in file sha256sum strings modinfo; do
    command -v "$cmd" >/dev/null || fail "Falta $cmd."
done

[[ -s "$MODULE" ]] ||
    fail "Falta el módulo compilado: $MODULE"
[[ -s "$CONFIG" ]] ||
    fail "Falta la configuración compilada: $CONFIG"
[[ -s "$PATCH_REPORT" ]] ||
    fail "Falta el informe de parche: $PATCH_REPORT"
[[ -s "$SD/cubegm/cores/lgpt_r36sx_port_libretro.so" ]] ||
    fail "Falta el core LGPT activo."
[[ -s "$BIN/r36s_u241_usb_audio_io" ]] ||
    fail "Falta el daemon de audio USB."
[[ -d "$MODROOT" ]] ||
    fail "Falta el árbol de módulos OTG."

DESC="$(file -b "$MODULE")"
VERMAGIC="$(modinfo -F vermagic "$MODULE" 2>/dev/null || true)"
echo "MODULE_FILE=$DESC"
echo "MODULE_VERMAGIC=$VERMAGIC"

[[ "$DESC" == *"ELF 32-bit"* &&
   "$DESC" == *"MIPS"* &&
   "$DESC" == *"relocatable"* ]] ||
    fail "Formato de módulo incompatible."

[[ "$VERMAGIC" == "4.4.186-release preempt MIPS32_R2 32BIT "* ||
   "$VERMAGIC" == "4.4.186-release preempt MIPS32_R2 32BIT" ]] ||
    fail "Vermagic incompatible: $VERMAGIC"

strings "$MODULE" | grep -Fq 'R36SX_U2414_AU8_SYNC_REPLICA' ||
    fail "Falta el marcador AU8-SYNC."
strings "$MODULE" | grep -Fq 'R36SX USB AUDIO' ||
    fail "Falta el nombre R36SX USB AUDIO."

python3 - "$MODULE" <<'PY'
from pathlib import Path
import sys

data = Path(sys.argv[1]).read_bytes()
checks = {
    "FS_OUT_SYNC": bytes.fromhex("07 05 00 0d ff 03 01"),
    "HS_OUT_SYNC": bytes.fromhex("07 05 00 0d 00 04 04"),
    "FS_IN_SYNC": bytes.fromhex("07 05 80 0d ff 03 01"),
}
for name, pattern in checks.items():
    print(f"{name}={data.count(pattern)}")

if data.count(checks["FS_OUT_SYNC"]) != 1:
    raise SystemExit("FS OUT synchronous descriptor invalid.")
if data.count(checks["HS_OUT_SYNC"]) < 1:
    raise SystemExit("HS OUT synchronous descriptor missing.")
if data.count(checks["FS_IN_SYNC"]) != 1:
    raise SystemExit("FS IN synchronous descriptor invalid.")

for pattern in (
    bytes.fromhex("07 05 00 05 ff 03 01"),
    bytes.fromhex("07 05 80 05 ff 03 01"),
):
    if pattern in data:
        raise SystemExit("An asynchronous Full-Speed endpoint remains.")

print("INSTALLER_AU8_SYNC_DESCRIPTOR_OK")
PY

# Preserve the current U2.41.3 runtime and module state.
for f in otg_u241_common.sh otg_u241_setup_once.sh \
         otg_u241_apply_profile_once.sh otg_u241_shutdown.sh; do
    [[ -e "$BIN/$f" ]] && cp -a "$BIN/$f" "$BACKUP/$f"
done

for olddir in \
  "$MODROOT/u2413_single_clock" \
  "$MODROOT/u2412_adaptive" \
  "$MODROOT/imported"; do
    [[ -d "$olddir" ]] && cp -a "$olddir" "$BACKUP/$(basename "$olddir")" ||
        true
done

[[ -e "$SD/lgpt/otg/audio_driver_mode" ]] &&
    cp -a "$SD/lgpt/otg/audio_driver_mode" "$BACKUP/audio_driver_mode" ||
    true

run_sd mkdir -p "$AU8DIR" "$BIN" "$LOGROOT" "$SD/lgpt/otg/logs"
run_sd cp -f "$MODULE" "$AU8DIR/usb_f_uac2.ko"
run_sd chmod 0644 "$AU8DIR/usb_f_uac2.ko"
run_sd cp -f "$CONFIG" "$AU8DIR/config_u2414_rt305x"
run_sd cp -f "$PATCH_REPORT" "$AU8DIR/PATCH_SOURCE_U2414.json"

for f in otg_u241_common.sh otg_u241_setup_once.sh \
         otg_u241_apply_profile_once.sh otg_u241_shutdown.sh; do
    run_sd cp -f "$ROOT/device/$f" "$BIN/$f"
    run_sd chmod 0755 "$BIN/$f"
done

printf 'LOCAL_CONSOLE\n' > /tmp/u2414_mode
run_sd cp -f /tmp/u2414_mode "$SD/lgpt/otg/audio_driver_mode"
run_sd touch "$SD/lgpt/otg/enable_lgpt_uac2_bridge"

cat > /tmp/U2414_AUDIO_DRIVER_README.txt <<'EOF'
LGPT U2.41.4 — REPLICA AU8-SYNC

Only the USB Audio kernel module and runtime are replaced.
LGPT core and sampler remain unchanged.

Historical Windows identity:
VID_1209&PID_38E8
Serial: R36SX-U2-38AU8-SYNC
Product: R36SX USB AUDIO

Transport:
48000 Hz
Mono
16-bit PCM
Synchronous USB Audio IN and OUT endpoints

Primary logs:
F:\LGPT_OTG_LOGS\U2414_AUDIO_DRIVER_SETUP.log
F:\LGPT_OTG_LOGS\U2414_AUDIO_DRIVER_SNAPSHOT.txt
F:\LGPT_OTG_LOGS\U2414_USB_AUDIO_DAEMON.log
EOF
run_sd cp -f /tmp/U2414_AUDIO_DRIVER_README.txt \
    "$LOGROOT/U2414_AUDIO_DRIVER_README.txt"

cat > "$PROJECT_ROOT/DIAGNOSTICO_WINDOWS_AUDIO_U2414.ps1" <<'POWERSHELL'
$root = "D:\R36S\PORT LPTRACKER\LOGS"
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$out = Join-Path $root "WINDOWS_AUDIO_U2414_$stamp"
New-Item -ItemType Directory -Force -Path $out | Out-Null

Get-PnpDevice |
  Where-Object {
    $_.InstanceId -match 'VID_1209&PID_38E8' -or
    $_.FriendlyName -match 'R36SX USB AUDIO'
  } |
  Format-List * |
  Out-File (Join-Path $out "pnp_r36sx_u2414.txt") -Width 600

Get-PnpDevice -Class MEDIA,AudioEndpoint |
  Format-List * |
  Out-File (Join-Path $out "audio_devices.txt") -Width 600

pnputil /enum-devices /class MEDIA |
  Out-File (Join-Path $out "pnputil_media.txt") -Width 600

pnputil /enum-devices /class AudioEndpoint |
  Out-File (Join-Path $out "pnputil_audioendpoint.txt") -Width 600

$setupApi = "$env:windir\INF\setupapi.dev.log"
if (Test-Path $setupApi) {
  Select-String -Path $setupApi `
    -Pattern 'VID_1209&PID_38E8','R36SX-U2-38AU8-SYNC','R36SX USB AUDIO' `
    -Context 50,100 |
    Out-File (Join-Path $out "setupapi_u2414.txt") -Width 700
}

$zip = "$out.zip"
Compress-Archive -Path "$out\*" -DestinationPath $zip -Force
Write-Host "WINDOWS_AUDIO_LOG=$zip"
POWERSHELL

cat > "$BACKUP/INSTALL_STATE_U2414.txt" <<EOF
Installed: $(date -Is)
Module: $AU8DIR/usb_f_uac2.ko
Module SHA256: $(sha256sum "$MODULE" | awk '{print $1}')
Vermagic: $VERMAGIC
Windows VID/PID: 1209:38E8
Serial: R36SX-U2-38AU8-SYNC
Product: R36SX USB AUDIO
Endpoint mode: synchronous IN and OUT
Profile: 48000 Hz mono 16-bit duplex
EOF
run_sd cp -f "$BACKUP/INSTALL_STATE_U2414.txt" \
    "$LOGROOT/INSTALL_STATE_U2414.txt"

A="$(sha256sum "$MODULE" | awk '{print $1}')"
B="$(sha256sum "$AU8DIR/usb_f_uac2.ko" | awk '{print $1}')"
[[ "$A" == "$B" ]] || fail "Hash del módulo copiado no coincide."

sync
echo "INSTALL_U2414_AU8_SYNC_OK"
echo "MODULE_SHA256=$B"
echo "MODULE_SD=$AU8DIR/usb_f_uac2.ko"
echo "BACKUP=$BACKUP"
echo "WINDOWS_DIAGNOSTIC=$PROJECT_ROOT/DIAGNOSTICO_WINDOWS_AUDIO_U2414.ps1"
echo "LOG=$LOG"
