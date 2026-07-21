param(
  [Parameter(Mandatory=$true)][string]$DriveLetter,
  [string]$SourceDir
)
$ErrorActionPreference = "Stop"
$drive = $DriveLetter.TrimEnd(':') + ':'
if (-not (Test-Path -LiteralPath $drive)) { throw "Drive not found: $drive" }
if (-not $SourceDir) { $SourceDir = Join-Path (Split-Path -Parent $PSScriptRoot) "scripts" }
$otg = Join-Path $drive "lgpt\otg"
$roms = Join-Path $drive "roms\lgpt"
$cubegm = Join-Path $drive "cubegm\lgpt"
New-Item -ItemType Directory -Force -Path $otg,$roms | Out-Null
Write-Host "== INSTALL U2.39A OTG KERNEL/MODULE PREP 60S =="
Write-Host "Drive: $drive"
Write-Host "SourceDir: $SourceDir"
Write-Host "OTG root: $otg"
$files = @(
  "u2_39_otg_common.sh",
  "U2_39A_KERNEL_MODULE_BUILD_AUDIT.sh",
  "U2_39B_DESCRIPTOR_RETRY_60S.sh",
  "U2_39C_MUSB_PULLUP_CYCLE_60S.sh",
  "U2_39D_RECOMMENDED_60S.sh",
  "U2_39E_COLLECT_DEVICE_LOG_BUNDLE.sh"
)
foreach ($f in $files) {
  $src = Join-Path $SourceDir $f
  $dst = Join-Path $otg $f
  Copy-Item -LiteralPath $src -Destination $dst -Force
  Write-Host "COPIED: $dst"
}
$docSrc = Join-Path (Split-Path -Parent $PSScriptRoot) "README_U2_39A_OTG_KERNEL_MODULE_PREP_60S_ES.md"
if (Test-Path -LiteralPath $docSrc) { Copy-Item -LiteralPath $docSrc -Destination (Join-Path $otg "README_U2_39A_OTG_KERNEL_MODULE_PREP_60S_ES.md") -Force }
function New-Launcher([string]$Name, [string]$Script, [string]$Args) {
  $path = Join-Path $roms $Name
  $body = "LGPT_OTG_SCRIPT`n/mnt/sdcard/lgpt/otg/$Script`n$Args`n"
  Set-Content -LiteralPath $path -Value $body -Encoding ASCII -NoNewline
  Write-Host "LAUNCHER: $path"
}
New-Launcher "OTG 0J Kernel Module Audit.lgpt" "U2_39A_KERNEL_MODULE_BUILD_AUDIT.sh" ""
New-Launcher "OTG 0K Descriptor Retry MTP 60s.lgpt" "U2_39B_DESCRIPTOR_RETRY_60S.sh" "mtp auto 60"
New-Launcher "OTG 0L Pullup Cycle 60s.lgpt" "U2_39C_MUSB_PULLUP_CYCLE_60S.sh" "60"
New-Launcher "OTG 0M U2.39 Recommended 60s.lgpt" "U2_39D_RECOMMENDED_60S.sh" ""
New-Launcher "OTG 0N Collect U2.39 Logs.lgpt" "U2_39E_COLLECT_DEVICE_LOG_BUNDLE.sh" ""
Write-Host "== OK =="
