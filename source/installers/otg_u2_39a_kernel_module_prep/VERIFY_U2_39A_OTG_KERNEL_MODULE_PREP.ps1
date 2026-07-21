param([Parameter(Mandatory=$true)][string]$DriveLetter)
$ErrorActionPreference = "Stop"
$drive = $DriveLetter.TrimEnd(':') + ':'
$required = @(
  "lgpt\otg\U2_39A_KERNEL_MODULE_BUILD_AUDIT.sh",
  "lgpt\otg\U2_39B_DESCRIPTOR_RETRY_60S.sh",
  "lgpt\otg\U2_39C_MUSB_PULLUP_CYCLE_60S.sh",
  "lgpt\otg\U2_39D_RECOMMENDED_60S.sh",
  "roms\lgpt\OTG 0J Kernel Module Audit.lgpt",
  "roms\lgpt\OTG 0K Descriptor Retry MTP 60s.lgpt",
  "roms\lgpt\OTG 0L Pullup Cycle 60s.lgpt",
  "roms\lgpt\OTG 0M U2.39 Recommended 60s.lgpt"
)
$missing = 0
foreach ($rel in $required) {
  $p = Join-Path $drive $rel
  if (Test-Path -LiteralPath $p) { Write-Host "OK: $p" } else { Write-Host "MISSING: $p"; $missing++ }
}
if ($missing -gt 0) { exit 1 }
Write-Host "OK: U2.39A verificado en $drive"
