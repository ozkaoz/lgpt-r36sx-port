param(
  [string]$DriveLetter = "F",
  [string]$OutputDir = "."
)
$ErrorActionPreference = "Stop"
function Normalize-Drive([string]$d) {
  $d = $d.Trim()
  if ($d.EndsWith(":")) { return $d }
  return "$d`:" 
}
$Drive = Normalize-Drive $DriveLetter
$Root = "$Drive\"
$Logs = Join-Path $Root "lgpt\otg\logs"
if (!(Test-Path -LiteralPath $Logs)) { throw "No existe $Logs" }
if (!(Test-Path -LiteralPath $OutputDir)) { New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null }
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$out = Join-Path $OutputDir "LGPT_U2_38_OTG_MODEKIT_LOGS_$stamp.zip"
if (Test-Path -LiteralPath $out) { Remove-Item -LiteralPath $out -Force }
Compress-Archive -Path (Join-Path $Logs "*") -DestinationPath $out -Force
Write-Host "OK_LOG_ZIP=$out"
