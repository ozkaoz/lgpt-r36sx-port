#!/usr/bin/env bash
set -euo pipefail

# LGPT R36SX AU11Z9
# Creates a reusable PC installer from a known-working SD installation.
# Default paths match: D:\R36S\PORT LPTRACKER

SD_DRIVE="${1:-F}"
OUT_BASE="${2:-/mnt/d/R36S/PORT LPTRACKER}"
PACKAGE_NAME="${3:-LGPT_R36SX_AU11Z9_PC_INSTALLER}"
INCLUDE_USER_DATA="${4:-minimal}"  # minimal | full

SD_DRIVE="${SD_DRIVE%:}"
SD="/mnt/${SD_DRIVE,,}"
OUT_DIR="$OUT_BASE/INSTALLERS/$PACKAGE_NAME"
PAYLOAD="$OUT_DIR/payload"
ZIP_PATH="$OUT_BASE/${PACKAGE_NAME}.zip"

fail() { echo "ERROR: $*" >&2; exit 1; }
copy_file() {
  local src="$1" dst="$2"
  [ -f "$src" ] || fail "missing required file: $src"
  mkdir -p "$(dirname "$dst")"
  cp -f --no-preserve=all "$src" "$dst" 2>/dev/null || cp -f "$src" "$dst"
}
copy_dir_minimal_lgpt() {
  local src="$1" dst="$2"
  mkdir -p "$dst"
  copy_file "$src/config.xml" "$dst/config.xml"
  for d in images exports chops tmp backups instruments projects samples otg; do
    mkdir -p "$dst/$d"
  done
  # Preserve lightweight installer/runtime notes when present.
  for f in AU11Z7_INSTALL_INFO.txt AU11Z8_SD_PERMISSION_FIX.txt; do
    [ -f "$src/$f" ] && copy_file "$src/$f" "$dst/$f"
  done
}
copy_dir_full() {
  local src="$1" dst="$2"
  mkdir -p "$(dirname "$dst")"
  rm -rf "$dst"
  cp -a --no-preserve=all "$src" "$dst" 2>/dev/null || cp -a "$src" "$dst"
}

[ -d "$SD" ] || fail "SD not mounted at $SD. Check Windows drive letter."
[ -d "$SD/cubegm" ] || fail "missing $SD/cubegm"
[ -d "$SD/cubegm/cores" ] || fail "missing $SD/cubegm/cores"
[ -d "$SD/roms/lgpt" ] || fail "missing $SD/roms/lgpt"
[ -d "$SD/lgpt" ] || fail "missing $SD/lgpt"
[ -f "$SD/cubegm/cores/lgpt_libretro.so" ] || fail "missing compiled core: $SD/cubegm/cores/lgpt_libretro.so"
[ -f "$SD/roms/lgpt/start.lgpt" ] || fail "missing $SD/roms/lgpt/start.lgpt"
[ -f "$SD/lgpt/config.xml" ] || fail "missing $SD/lgpt/config.xml"

rm -rf "$OUT_DIR"
mkdir -p "$PAYLOAD/cubegm/cores" "$PAYLOAD/roms/lgpt" "$PAYLOAD/lgpt" "$OUT_DIR/logs"

copy_file "$SD/cubegm/cores/lgpt_libretro.so" "$PAYLOAD/cubegm/cores/lgpt_libretro.so"
[ -f "$SD/cubegm/lgpt_libretro.so" ] && copy_file "$SD/cubegm/lgpt_libretro.so" "$PAYLOAD/cubegm/lgpt_libretro.so" || copy_file "$SD/cubegm/cores/lgpt_libretro.so" "$PAYLOAD/cubegm/lgpt_libretro.so"
[ -f "$SD/cubegm/lgpt" ] && copy_file "$SD/cubegm/lgpt" "$PAYLOAD/cubegm/lgpt"
[ -f "$SD/cubegm/lgpt.elf" ] && copy_file "$SD/cubegm/lgpt.elf" "$PAYLOAD/cubegm/lgpt.elf"
copy_file "$SD/roms/lgpt/start.lgpt" "$PAYLOAD/roms/lgpt/start.lgpt"
[ -f "$SD/roms/lgpt/filelist.csv" ] && copy_file "$SD/roms/lgpt/filelist.csv" "$PAYLOAD/roms/lgpt/filelist.csv" || printf 'start.lgpt\n' > "$PAYLOAD/roms/lgpt/filelist.csv"
[ -f "$SD/roms/lgpt/README_LGPT_R36SX.txt" ] && copy_file "$SD/roms/lgpt/README_LGPT_R36SX.txt" "$PAYLOAD/roms/lgpt/README_LGPT_R36SX.txt"

if [ "${INCLUDE_USER_DATA,,}" = "full" ]; then
  copy_dir_full "$SD/lgpt" "$PAYLOAD/lgpt"
else
  copy_dir_minimal_lgpt "$SD/lgpt" "$PAYLOAD/lgpt"
fi

CORE_SHA="$(sha256sum "$PAYLOAD/cubegm/cores/lgpt_libretro.so" | awk '{print $1}')"
cat > "$OUT_DIR/MANIFEST_AU11Z9.txt" <<MANIFEST
LGPT_R36SX_AU11Z9_PC_INSTALLER
Created: $(date -Iseconds)
Source SD: $SD
Mode: $INCLUDE_USER_DATA
Core: payload/cubegm/cores/lgpt_libretro.so
Core SHA256: $CORE_SHA
Required target SD folders: cubegm, cubegm/cores, frogui, roms
Installs: /lgpt, /roms/lgpt/start.lgpt, /cubegm/cores/lgpt_libretro.so, /cubegm/lgpt_libretro.so, /cubegm/lgpt, /cubegm/lgpt.elf
MANIFEST

cat > "$OUT_DIR/INSTALL_LGPT_R36SX_TO_SD.ps1" <<'PS1'
param(
    [string]$SdDrive = "F",
    [switch]$Force
)
$ErrorActionPreference = "Stop"
$SdDrive = $SdDrive.TrimEnd(':')
$Root = "$SdDrive`:"
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Payload = Join-Path $ScriptRoot "payload"

function Fail($m) { Write-Host "ERROR: $m" -ForegroundColor Red; exit 1 }
function NeedDir($p) { if (!(Test-Path $p -PathType Container)) { Fail "No existe: $p" } }
function MkDir($p) { if (!(Test-Path $p)) { New-Item -ItemType Directory -Force -Path $p | Out-Null } }
function CopyPayloadFile($rel) {
    $src = Join-Path $Payload $rel
    $dst = Join-Path $Root $rel
    if (!(Test-Path $src -PathType Leaf)) { Fail "Falta payload: $src" }
    MkDir (Split-Path -Parent $dst)
    Copy-Item -Force $src $dst
}
function CopyPayloadDirContents($rel) {
    $src = Join-Path $Payload $rel
    $dst = Join-Path $Root $rel
    if (!(Test-Path $src -PathType Container)) { Fail "Falta payload: $src" }
    MkDir $dst
    Copy-Item -Force -Recurse (Join-Path $src "*") $dst
}

NeedDir $Root
NeedDir (Join-Path $Root "cubegm")
NeedDir (Join-Path $Root "cubegm\cores")
NeedDir (Join-Path $Root "frogui")
NeedDir (Join-Path $Root "roms")

Write-Host "Instalando LGPT R36SX en $Root ..."
MkDir (Join-Path $Root "lgpt")
MkDir (Join-Path $Root "roms\lgpt")
MkDir (Join-Path $Root "cubegm\cores")

CopyPayloadDirContents "lgpt"
CopyPayloadDirContents "roms\lgpt"
CopyPayloadFile "cubegm\cores\lgpt_libretro.so"
CopyPayloadFile "cubegm\lgpt_libretro.so"
if (Test-Path (Join-Path $Payload "cubegm\lgpt")) { CopyPayloadFile "cubegm\lgpt" }
if (Test-Path (Join-Path $Payload "cubegm\lgpt.elf")) { CopyPayloadFile "cubegm\lgpt.elf" }

$required = @(
    "lgpt\config.xml",
    "roms\lgpt\start.lgpt",
    "cubegm\cores\lgpt_libretro.so"
)
foreach ($r in $required) {
    if (!(Test-Path (Join-Path $Root $r))) { Fail "No se instaló: $r" }
}
Write-Host "OK: LGPT instalado. Expulsa la SD de forma segura antes de probar en la consola." -ForegroundColor Green
PS1

cat > "$OUT_DIR/VERIFY_LGPT_R36SX_SD.ps1" <<'PS1'
param([string]$SdDrive = "F")
$SdDrive = $SdDrive.TrimEnd(':')
$Root = "$SdDrive`:"
$checks = @(
  "cubegm",
  "cubegm\cores",
  "frogui",
  "roms",
  "roms\lgpt",
  "roms\lgpt\start.lgpt",
  "lgpt",
  "lgpt\config.xml",
  "cubegm\cores\lgpt_libretro.so",
  "cubegm\lgpt_libretro.so",
  "cubegm\lgpt",
  "cubegm\lgpt.elf"
)
$ok = $true
foreach ($c in $checks) {
  $p = Join-Path $Root $c
  if (Test-Path $p) { Write-Host "[OK] $c" -ForegroundColor Green }
  else { Write-Host "[FAIL] $c" -ForegroundColor Red; $ok = $false }
}
if ($ok) { exit 0 } else { exit 1 }
PS1

cat > "$OUT_DIR/install_from_wsl.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
SD_DRIVE="${1:-F}"; SD_DRIVE="${SD_DRIVE%:}"
SD="/mnt/${SD_DRIVE,,}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PAYLOAD="$HERE/payload"
fail(){ echo "ERROR: $*" >&2; exit 1; }
copy_tree(){ mkdir -p "$2"; cp -a --no-preserve=all "$1"/. "$2"/ 2>/dev/null || cp -a "$1"/. "$2"/; }
copy_file(){ mkdir -p "$(dirname "$2")"; cp -f --no-preserve=all "$1" "$2" 2>/dev/null || cp -f "$1" "$2"; }
[ -d "$SD" ] || fail "SD not mounted: $SD"
[ -d "$SD/cubegm/cores" ] || fail "missing $SD/cubegm/cores"
[ -d "$SD/frogui" ] || fail "missing $SD/frogui"
[ -d "$SD/roms" ] || fail "missing $SD/roms"
mkdir -p "$SD/lgpt" "$SD/roms/lgpt" "$SD/cubegm/cores"
copy_tree "$PAYLOAD/lgpt" "$SD/lgpt"
copy_tree "$PAYLOAD/roms/lgpt" "$SD/roms/lgpt"
copy_file "$PAYLOAD/cubegm/cores/lgpt_libretro.so" "$SD/cubegm/cores/lgpt_libretro.so"
copy_file "$PAYLOAD/cubegm/lgpt_libretro.so" "$SD/cubegm/lgpt_libretro.so"
[ -f "$PAYLOAD/cubegm/lgpt" ] && copy_file "$PAYLOAD/cubegm/lgpt" "$SD/cubegm/lgpt"
[ -f "$PAYLOAD/cubegm/lgpt.elf" ] && copy_file "$PAYLOAD/cubegm/lgpt.elf" "$SD/cubegm/lgpt.elf"
sync
echo "OK: LGPT installed to $SD"
SH
chmod +x "$OUT_DIR/install_from_wsl.sh"

cat > "$OUT_DIR/README_REINSTALACION_LGPT_R36SX_AU11Z9.md" <<'README'
# Instalador LGPT R36SX AU11Z9

Este paquete reinstala el port de LGPT en una SD ya preparada con Stock OS + TreeFrogUI.

## Desde Windows PowerShell

Abrir PowerShell en esta carpeta y ejecutar estos comandos, cambiando F por la letra real de la SD:

    powershell -ExecutionPolicy Bypass -File .\INSTALL_LGPT_R36SX_TO_SD.ps1 -SdDrive F
    powershell -ExecutionPolicy Bypass -File .\VERIFY_LGPT_R36SX_SD.ps1 -SdDrive F

## Desde WSL Ubuntu

Ejecutar, cambiando F por la letra real de la SD:

    cd "/mnt/d/R36S/PORT LPTRACKER/INSTALLERS/LGPT_R36SX_AU11Z9_PC_INSTALLER"
    bash install_from_wsl.sh F

## Qué instala

- F:\lgpt
- F:\roms\lgpt\start.lgpt
- F:\cubegm\cores\lgpt_libretro.so
- F:\cubegm\lgpt_libretro.so
- F:\cubegm\lgpt
- F:\cubegm\lgpt.elf

No formatea ni toca el sistema base. Requiere que la SD ya tenga cubegm, frogui y roms.
README

( cd "$OUT_DIR" && find payload -type f -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS_AU11Z9_PAYLOAD.txt )
rm -f "$ZIP_PATH"
if command -v zip >/dev/null 2>&1; then
  ( cd "$OUT_BASE/INSTALLERS" && zip -qr "$ZIP_PATH" "$PACKAGE_NAME" )
else
  python3 - <<PY
import zipfile, os
out = r'''$ZIP_PATH'''
root = r'''$OUT_BASE/INSTALLERS'''
name = r'''$PACKAGE_NAME'''
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    base = os.path.join(root, name)
    for dp, dn, fn in os.walk(base):
        for f in fn:
            p = os.path.join(dp, f)
            z.write(p, os.path.relpath(p, root))
PY
fi

echo "SUMMARY=PASS_AU11Z9_PC_INSTALLER_CREATED"
echo "INSTALLER_DIR=$OUT_DIR"
echo "INSTALLER_ZIP=$ZIP_PATH"
echo "CORE_SHA256=$CORE_SHA"
