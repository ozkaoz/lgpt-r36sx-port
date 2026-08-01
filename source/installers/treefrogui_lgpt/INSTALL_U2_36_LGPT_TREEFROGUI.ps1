<#
Instalador LGPT U2.36 para TreeFrogUI 1.0.0 / R36S.

Objetivo:
- Usar la carpeta LGPT nativa de TreeFrogUI: roms\LGPT.
- Crear carpeta de datos LGPT en la raíz de la SD: LGPT\projects, samples, instruments, etc.
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

# Limpieza de launchers experimentales anteriores creados durante diagnóstico.
$oldLaunchers = @(
    "$Drive\GBA\LGPT_U2_36.gba",
    "$Drive\lgpt\Start LGPT.gba",
    "$Drive\LGPT\Start LGPT.gba",
    "$Drive\LGPT\LGPT_U2_36.gba"
)
foreach ($old in $oldLaunchers) {
    if (Test-Path -LiteralPath $old) {
        Remove-Item -LiteralPath $old -Force
        Write-Host "Eliminado launcher anterior: $old"
    }
}

# Carpeta de datos en la raíz de la SD. En config.xml se usa /mnt/sdcard/lgpt;
# en FAT/VFAT el acceso no distingue mayúsculas/minúsculas, pero se crea como LGPT
# para coincidir con la categoría visible.
$dataDir = "$Drive\LGPT"
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

  <BACKGROUND value="0A0A18" />
  <FOREGROUND value="E8E4F8" />
  <BORDER value="3F5FBF" />
  <SONGVIEW_FE value="2A3E8F" />
  <SONGVIEW_00 value="1E2B66" />
  <HICOLOR1 value="5B8CFF" />
  <HICOLOR2 value="9D5BFF" />
  <CURSORCOLOR value="7FB8FF" />
  <PLAYCOLOR value="4AD8FF" />
  <MUTECOLOR value="8890B0" />
  <ROWCOLOR1 value="5A7DF0" />
  <ROWCOLOR2 value="A86BFF" />
  <MAJORBEAT value="5A7DF0" />
</CONFIG>
'@
Write-AsciiFile (Join-Path $dataDir "config.xml") $config

# Carpeta visible nativa TreeFrogUI: roms\LGPT.
$romsLgpt = "$Drive\roms\LGPT"
New-Item -ItemType Directory -Force -Path $romsLgpt | Out-Null
New-Item -ItemType Directory -Force -Path "$romsLgpt\.res" | Out-Null

$launcherName = "LGPT_U2_36.lgpt"
$launcherPath = Join-Path $romsLgpt $launcherName
$launcherContent = @'
LGPT U2.36 launcher for TreeFrogUI 1.0.0.
This file is intentionally small; TreeFrogUI routes it to cubegm/cores/lgpt_libretro.so.
LGPT data lives in /mnt/sdcard/lgpt.
'@
Write-AsciiFile $launcherPath $launcherContent

# CSV local para que el título se vea limpio dentro de LGPT.
Write-AsciiFile (Join-Path $romsLgpt "filelist.csv") "LGPT_U2_36.lgpt,LGPT U2.36,LGPT U2.36"

# También dejamos un README dentro de la carpeta LGPT visible.
$romsReadme = @'
LGPT U2.36

Pulsa A sobre LGPT U2.36 para arrancar el port.
No borres la carpeta de datos /LGPT de la raíz: contiene projects, samples, instruments y config.xml.
'@
Write-AsciiFile (Join-Path $romsLgpt "README_LGPT_U2_36.txt") $romsReadme

# Registro en allfiles.lst. TreeFrogUI normalmente escanea roms/, pero allfiles ayuda en builds/cacheados.
$allfiles = "$Drive\cubegm\allfiles.lst"
Backup-IfExists $allfiles $ts
$rel = "roms/LGPT/$launcherName"
if (Test-Path -LiteralPath $allfiles) {
    $lines = Get-Content -LiteralPath $allfiles -ErrorAction SilentlyContinue
    $lines = $lines | Where-Object {
        $_ -notmatch "LGPT_U2_36" -and
        $_ -notmatch "Start LGPT" -and
        $_ -notmatch "^GBA/LGPT" -and
        $_ -notmatch "^lgpt/Start"
    }
    @($lines + "$rel|LGPT U2.36|LGPT U2.36|LGPT U2.36|LGPT") | Set-Content -LiteralPath $allfiles -Encoding ASCII -Force
} else {
    "$rel|LGPT U2.36|LGPT U2.36|LGPT U2.36|LGPT" | Set-Content -LiteralPath $allfiles -Encoding ASCII -Force
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

$entry = '  <file name="roms/LGPT/LGPT_U2_36.lgpt" core="lgpt_libretro.so" />'
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
LGPT U2.36 installed for TreeFrogUI 1.0.0
Installed: $(Get-Date -Format s)
Core: cubegm/cores/lgpt_libretro.so
Visible launcher: roms/LGPT/LGPT_U2_36.lgpt
Data root: LGPT/
Required folders: LGPT/projects, LGPT/samples, LGPT/instruments, LGPT/images, LGPT/exports, LGPT/chops, LGPT/tmp, LGPT/backups
"@
Write-AsciiFile "$dataDir\INSTALL_U2_36_TREEFROGUI.txt" $installInfo

# Verificación final.
Write-Host ""
Write-Host "== VERIFICACION FINAL =="
Get-Item -LiteralPath $coreDst | Format-List FullName,Length,LastWriteTime
Write-Host ""
Write-Host "Datos LGPT raíz:"
Get-ChildItem -LiteralPath $dataDir -Force | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
Write-Host ""
Write-Host "LGPT visible en roms:"
Get-ChildItem -LiteralPath $romsLgpt -Force | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
Write-Host ""
Write-Host "Override core:"
Select-String -LiteralPath $coreFilelist -Pattern "LGPT|lgpt" | ForEach-Object { $_.Line }
Write-Host ""
Write-Host "Allfiles:"
Select-String -LiteralPath $allfiles -Pattern "LGPT|lgpt" | ForEach-Object { $_.Line }
Write-Host ""
Write-Host "OK: Instalación LGPT U2.36 lista. En TreeFrogUI entra por LGPT -> LGPT U2.36."
