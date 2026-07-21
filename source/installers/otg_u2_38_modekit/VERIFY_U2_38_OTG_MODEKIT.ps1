param([string]$DriveLetter = "F")
$ErrorActionPreference = "Stop"
function Normalize-Drive([string]$d) { if ($d.Trim().EndsWith(":")) { return $d.Trim() } else { return "$($d.Trim())`:" } }
$Drive = Normalize-Drive $DriveLetter
$Root = "$Drive\"
$OtgRoot = Join-Path $Root "lgpt\otg"
$required = @(
  "u2_38_otg_common.sh",
  "U2_38A_OTG_STATE_SNAPSHOT.sh",
  "U2_38B_OTG_FILE_ACCESS_MODE.sh",
  "U2_38C_UAC_AUDIO_GADGET_MODE.sh",
  "U2_38D_USB_AUDIO_HOST_PROBE.sh",
  "U2_38E_COLLECT_DEVICE_LOG_BUNDLE.sh",
  "LEEME_U2_38_OTG_MODEKIT_SD.txt"
)
foreach ($r in $required) {
  $p = Join-Path $OtgRoot $r
  if (!(Test-Path -LiteralPath $p)) { throw "Falta $p" }
}
Write-Host "OK: U2.38 OTG Modekit instalado en $OtgRoot"
