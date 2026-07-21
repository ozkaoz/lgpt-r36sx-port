param(
  [Parameter(Mandatory=$true)][string]$DriveLetter,
  [string]$OutputDir
)
$ErrorActionPreference = "Stop"
$drive = $DriveLetter.TrimEnd(':') + ':'
if (-not $OutputDir) { $OutputDir = Join-Path (Get-Location) "LOGS" }
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$zip = Join-Path $OutputDir "LGPT_U2_39A_OTG_LOGS_$stamp.zip"
$tmp = Join-Path $env:TEMP "LGPT_U2_39A_OTG_LOGS_$stamp"
Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
$logdir = Join-Path $drive "lgpt\otg\logs"
$otgdir = Join-Path $drive "lgpt\otg"
if (Test-Path -LiteralPath $logdir) { Copy-Item -LiteralPath $logdir -Destination (Join-Path $tmp "logs") -Recurse -Force }
if (Test-Path -LiteralPath $otgdir) {
  New-Item -ItemType Directory -Force -Path (Join-Path $tmp "otg_root_snapshot") | Out-Null
  Get-ChildItem -LiteralPath $otgdir -File | Copy-Item -Destination (Join-Path $tmp "otg_root_snapshot") -Force
}
Compress-Archive -LiteralPath (Join-Path $tmp "*") -DestinationPath $zip -Force
Write-Host "OK: $zip"
