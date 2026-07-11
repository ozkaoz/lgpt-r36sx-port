<#
LGPT U2.41 installer for TreeFrogUI 1.0.0 / R36S.

Objetivo:
- Use a single launcher only: roms\lgpt\start.lgpt.
- Keep LGPT data in the SD root: lgpt\projects, samples, instruments, exports, etc.
- Copiar/registrar el core lgpt_libretro.so.

Uso desde PowerShell:
  powershell -ExecutionPolicy Bypass -File .\INSTALL_U2_36_LGPT_TREEFROGUI.ps1 -Drive F: -CorePath C:\ruta\lgpt_libretro.so

Uso si el core ya fue copiado:
  powershell -ExecutionPolicy Bypass -File .\INSTALL_U2_36_LGPT_TREEFROGUI.ps1 -Drive F:
#>
param(
    [string]$Drive = "F:",
    [string]$CorePath = "",
    [switch]$SkipCoreCopy
)

$ErrorActionPreference = "Stop"

function Normalize-Drive([string]$d) {
    if ($d.Length -eq 1) { return ($d + ":") }
    return $d.TrimEnd('\')
}

function Backup-IfExists([string]$path, [string]$ts) {
    if (Test-Path -LiteralPath $path) {
        Copy-Item -LiteralPath $path -Destination "$path.bak_lgpt_u236_$ts" -Force
    }
}

function Write-AsciiFile([string]$path, [string]$content) {
    $parent = Split-Path -Parent $path
    if ($parent -and !(Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    $content | Set-Content -LiteralPath $path -Encoding ASCII -Force
}

$Drive = Normalize-Drive $Drive
$Root = "$Drive\"
$ts = Get-Date -Format "yyyyMMdd_HHmmss"

Write-Host "== INSTALADOR LGPT U2.36 / TREEFROGUI LGPT =="
Write-Host "Drive=$Drive"
Write-Host "CorePath=$CorePath"
Write-Host "SkipCoreCopy=$SkipCoreCopy"
Write-Host ""

if (!(Test-Path -LiteralPath $Root)) {
    throw "No existe la unidad $Drive. Revisa la letra de la SD."
}
if (!(Test-Path -LiteralPath "$Drive\cubegm")) {
    throw "No existe $Drive\cubegm. La SD no parece tener TreeFrogUI instalado."
}
if (!(Test-Path -LiteralPath "$Drive\frogui")) {
    Write-Host "AVISO: No existe $Drive\frogui. Continúo, pero revisa la instalación de TreeFrogUI."
}
if (!(Test-Path -LiteralPath "$Drive\roms")) {
    New-Item -ItemType Directory -Force -Path "$Drive\roms" | Out-Null
}
if (!(Test-Path -LiteralPath "$Drive\cubegm\cores")) {
    New-Item -ItemType Directory -Force -Path "$Drive\cubegm\cores" | Out-Null
}

$coreDst = "$Drive\cubegm\cores\lgpt_libretro.so"

if (!$SkipCoreCopy) {
    if ($CorePath -and (Test-Path -LiteralPath $CorePath)) {
        Copy-Item -LiteralPath $CorePath -Destination $coreDst -Force
        Write-Host "Core copiado a: $coreDst"
    } elseif (Test-Path -LiteralPath $coreDst) {
        Write-Host "Core ya existe en SD: $coreDst"
    } else {
        throw "No hay core. Pasa -CorePath C:\ruta\lgpt_libretro.so o copia primero $coreDst."
    }
} else {
    if (!(Test-Path -LiteralPath $coreDst)) {
        throw "SkipCoreCopy está activo, pero no existe $coreDst."
    }
}

# Remove every previous LGPT launcher made by older test installers.
$oldLaunchers = @(
    "$Drive\GBA\LGPT_U2_36.gba",
    "$Drive\lgpt\Start LGPT.gba",
    "$Drive\LGPT\Start LGPT.gba",
    "$Drive\LGPT\LGPT_U2_36.gba",
    "$Drive\roms\LGPT\LGPT_U2_36.lgpt",
    "$Drive\roms\LGPT\start.lgpt",
    "$Drive\roms\LGPT\start.lgpt",
    "$Drive\roms\lgpt\LGPT_U2_36.lgpt",
    "$Drive\roms\lgpt\start.lgpt"
)
foreach ($old in $oldLaunchers) {
    if (Test-Path -LiteralPath $old) {
        Remove-Item -LiteralPath $old -Force
        Write-Host "Removed old launcher: $old"
    }
}

# Data folder in the SD root. config.xml uses /mnt/sdcard/lgpt.
$dataDir = "$Drive\lgpt"
New-Item -ItemType Directory -Force -Path $dataDir | Out-Null
foreach ($sub in @("projects", "samples", "instruments", "images", "exports", "chops", "tmp", "backups")) {
    New-Item -ItemType Directory -Force -Path (Join-Path $dataDir $sub) | Out-Null
}

$config = @'
<CONFIG>
  <ROOTFOLDER value="/mnt/sdcard/lgpt" />
  <SAMPLELIB value="/mnt/sdcard/lgpt/samples" />
  <INSTRUMENTFOLDER value="/mnt/sdcard/lgpt/instruments" />

  <AUDIOAPI value="libretro" />
  <AUDIODEVICE value="picoarch" />
  <AUDIOBUFFERSIZE value="1024" />
  <AUDIOPREBUFFERCOUNT value="4" />
  <AUTO_LOAD_LAST value="NO" />

  <BACKGROUND value="1D0A1F" />
  <FOREGROUND value="F5EBFF" />
  <BORDER value="FF008C" />
  <SONGVIEW_FE value="A55B8F" />
  <SONGVIEW_00 value="853B6F" />
  <HICOLOR1 value="B750D1" />
  <HICOLOR2 value="DB33DB" />
  <CURSORCOLOR value="FF008C" />
  <PLAYCOLOR value="FF008C" />
  <MUTECOLOR value="F5EBFF" />
  <ROWCOLOR1 value="BA28F9" />
  <ROWCOLOR2 value="FF00FF" />
  <MAJORBEAT value="BA28F9" />
</CONFIG>
'@
Write-AsciiFile (Join-Path $dataDir "config.xml") $config

# Single visible TreeFrogUI launcher: roms\lgpt\start.lgpt.
$romsLgpt = "$Drive\roms\lgpt"
New-Item -ItemType Directory -Force -Path $romsLgpt | Out-Null
New-Item -ItemType Directory -Force -Path "$romsLgpt\.res" | Out-Null

$launcherName = "start.lgpt"
$launcherPath = Join-Path $romsLgpt $launcherName
$launcherContent = @'
LGPT launcher for TreeFrogUI.
This is the only supported entry point for the port.
LGPT data lives in /mnt/sdcard/lgpt.
'@
Write-AsciiFile $launcherPath $launcherContent

# Local CSV for a clean title while keeping the physical launcher name stable.
Write-AsciiFile (Join-Path $romsLgpt "filelist.csv") "start.lgpt,LGPT,LGPT"

$romsReadme = @'
LGPT

Start the port only through start.lgpt.
Do not delete the SD root /lgpt folder; it contains projects, samples, instruments, exports and config.xml.
'@
Write-AsciiFile (Join-Path $romsLgpt "README_LGPT.txt") $romsReadme

# Registro en allfiles.lst. TreeFrogUI normalmente escanea roms/, pero allfiles ayuda en builds/cacheados.
$allfiles = "$Drive\cubegm\allfiles.lst"
Backup-IfExists $allfiles $ts
$rel = "roms/lgpt/$launcherName"
if (Test-Path -LiteralPath $allfiles) {
    $lines = Get-Content -LiteralPath $allfiles -ErrorAction SilentlyContinue
    $lines = $lines | Where-Object {
        $_ -notmatch "LGPT_U2_36" -and
        $_ -notmatch "Start LGPT" -and
        $_ -notmatch "^GBA/LGPT" -and
        $_ -notmatch "^lgpt/Start" -and
        $_ -notmatch "^roms/LGPT/" -and
        $_ -notmatch "^roms/lgpt/"
    }
    @($lines + "$rel|LGPT|LGPT|LGPT|LGPT") | Set-Content -LiteralPath $allfiles -Encoding ASCII -Force
} else {
    "$rel|LGPT|LGPT|LGPT|LGPT" | Set-Content -LiteralPath $allfiles -Encoding ASCII -Force
}

# Registro exacto de core para el launcher LGPT.
$coreFilelist = "$Drive\cubegm\cores\filelist.xml"
Backup-IfExists $coreFilelist $ts
if (Test-Path -LiteralPath $coreFilelist) {
    $xml = Get-Content -LiteralPath $coreFilelist -Raw -ErrorAction SilentlyContinue
} else {
    $xml = "<files>`r`n</files>`r`n"
}

# Limpieza de overrides de diagnóstico anteriores.
$xml = $xml -replace '(?m)^\s*<file name="GBA/LGPT_U2_36\.gba" core="lgpt_libretro\.so" />\s*\r?\n?', ''
$xml = $xml -replace '(?m)^\s*<file name="LGPT/LGPT_U2_36\.gba" core="lgpt_libretro\.so" />\s*\r?\n?', ''
$xml = $xml -replace '(?m)^\s*<file name="lgpt/Start LGPT\.gba" core="lgpt_libretro\.so" />\s*\r?\n?', ''
$xml = $xml -replace '(?m)^\s*<file name="roms/LGPT/LGPT_U2_36\.lgpt" core="lgpt_libretro\.so" />\s*\r?\n?', ''
$xml = $xml -replace '(?m)^\s*<file name="roms/LGPT/[^"]+\.lgpt" core="lgpt_libretro\.so" />\s*\r?\n?', ''
$xml = $xml -replace '(?m)^\s*<file name="roms/lgpt/[^"]+\.lgpt" core="lgpt_libretro\.so" />\s*\r?\n?', ''

$entry = '  <file name="roms/lgpt/start.lgpt" core="lgpt_libretro.so" />'
if ($xml -match '</files>') {
    $xml = $xml -replace '</files>', "$entry`r`n</files>"
} elseif ($xml -match '</filelist>') {
    $xml = $xml -replace '</filelist>', "$entry`r`n</filelist>"
} else {
    $xml = $xml.TrimEnd() + "`r`n" + $entry + "`r`n"
}
$xml | Set-Content -LiteralPath $coreFilelist -Encoding ASCII -Force

# Marcador en raíz para diagnóstico humano.
$installInfo = @"
LGPT U2.41 installed for TreeFrogUI 1.0.0
Installed: $(Get-Date -Format s)
Core: cubegm/cores/lgpt_libretro.so
Visible launcher: roms/lgpt/start.lgpt
Data root: lgpt/
Required folders: lgpt/projects, lgpt/samples, lgpt/instruments, lgpt/images, lgpt/exports, lgpt/chops, lgpt/tmp, lgpt/backups
"@
Write-AsciiFile "$dataDir\INSTALL_U2_36_TREEFROGUI.txt" $installInfo

# Verificación final.
Write-Host ""
Write-Host "== VERIFICACION FINAL =="
Get-Item -LiteralPath $coreDst | Format-List FullName,Length,LastWriteTime
Write-Host ""
Write-Host "LGPT data root:"
Get-ChildItem -LiteralPath $dataDir -Force | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
Write-Host ""
Write-Host "Visible launcher folder:"
Get-ChildItem -LiteralPath $romsLgpt -Force | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
Write-Host ""
Write-Host "Override core:"
Select-String -LiteralPath $coreFilelist -Pattern "LGPT|lgpt" | ForEach-Object { $_.Line }
Write-Host ""
Write-Host "Allfiles:"
Select-String -LiteralPath $allfiles -Pattern "LGPT|lgpt" | ForEach-Object { $_.Line }
Write-Host ""
Write-Host "OK: LGPT U2.41 installed. Start it only through roms/lgpt/start.lgpt."
