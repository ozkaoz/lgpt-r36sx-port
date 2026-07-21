param(
  [string]$DriveLetter = "F",
  [Parameter(Mandatory=$true)][string]$SourceDir,
  [Parameter(Mandatory=$true)][string]$DocsDir
)

$ErrorActionPreference = "Stop"
function Normalize-Drive([string]$d) {
  $d = $d.Trim()
  if ($d.EndsWith(":")) { return $d }
  return "$d`:" 
}
function Ensure-Dir([string]$Path) {
  if (!(Test-Path -LiteralPath $Path)) { New-Item -ItemType Directory -Force -Path $Path | Out-Null }
}
function Write-Utf8NoBom([string]$Path, [string]$Text) {
  $enc = New-Object System.Text.UTF8Encoding($false)
  [System.IO.File]::WriteAllText($Path, $Text, $enc)
}

$Drive = Normalize-Drive $DriveLetter
$Root = "$Drive\"
if (!(Test-Path -LiteralPath $Root)) { throw "No existe la unidad $Drive" }
if (!(Test-Path -LiteralPath (Join-Path $Root "lgpt"))) { throw "No existe $Drive\lgpt. Instala/verifica primero LGPT U2.36." }

$OtgRoot = Join-Path $Root "lgpt\otg"
$Logs = Join-Path $OtgRoot "logs"
Ensure-Dir $OtgRoot
Ensure-Dir $Logs

Write-Host "== INSTALL LGPT U2.38 OTG MODEKIT =="
Write-Host "Drive: $Drive"
Write-Host "SourceDir: $SourceDir"
Write-Host "OTG root: $OtgRoot"

Get-ChildItem -LiteralPath $SourceDir -File | ForEach-Object {
  Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $OtgRoot $_.Name) -Force
}
Get-ChildItem -LiteralPath $DocsDir -File | ForEach-Object {
  Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $OtgRoot $_.Name) -Force
}

$quick = @"
LGPT R36SX U2.38 OTG MODEKIT

Ruta en consola: /mnt/sdcard/lgpt/otg

Secuencia recomendada:
  cd /mnt/sdcard/lgpt/otg
  sh ./U2_38A_OTG_STATE_SNAPSHOT.sh
  sh ./U2_38C_UAC_AUDIO_GADGET_MODE.sh 120 auto
  sh ./U2_38D_USB_AUDIO_HOST_PROBE.sh 60

Modo archivos seguro PC -> SD:
  cd /mnt/sdcard/lgpt/otg
  sh ./U2_38B_OTG_FILE_ACCESS_MODE.sh 300 128 image

Durante el modo archivo, Windows debe ver un disco USB pequeño.
Copia archivos dentro de PC_TO_LGPT/SAMPLES, PC_TO_LGPT/DOCS o PC_TO_LGPT/PROJECTS.
Expulsa/eject el disco desde Windows antes de que termine la ventana.
Al cerrar, el script importa a:
  /mnt/sdcard/lgpt/inbox_pc
  /mnt/sdcard/lgpt/samples/otg_import
  /mnt/sdcard/lgpt/documents
  /mnt/sdcard/lgpt/projects/otg_import

Logs:
  /mnt/sdcard/lgpt/otg/logs
"@
Write-Utf8NoBom (Join-Path $OtgRoot "LEEME_U2_38_OTG_MODEKIT_SD.txt") $quick

Write-Host "== OK =="
Write-Host "Instalado en $OtgRoot"
