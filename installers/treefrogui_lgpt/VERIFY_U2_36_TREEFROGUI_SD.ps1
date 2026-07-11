param([string]$Drive = "F:")
$ErrorActionPreference = "Stop"
if ($Drive.Length -eq 1) { $Drive = "${Drive}:" }
Write-Host "== VERIFICAR SD LGPT U2.36 =="
$paths = @(
  "$Drive\cubegm\cores\lgpt_libretro.so",
  "$Drive\LGPT",
  "$Drive\LGPT\projects",
  "$Drive\LGPT\samples",
  "$Drive\LGPT\instruments",
  "$Drive\LGPT\images",
  "$Drive\LGPT\config.xml",
  "$Drive\roms\LGPT",
  "$Drive\roms\LGPT\LGPT_U2_36.lgpt",
  "$Drive\roms\LGPT\filelist.csv",
  "$Drive\cubegm\cores\filelist.xml",
  "$Drive\cubegm\allfiles.lst"
)
foreach ($p in $paths) {
  if (Test-Path -LiteralPath $p) { Write-Host "OK   $p" } else { Write-Host "MISS $p" }
}
Write-Host ""
Write-Host "Core override:"
Select-String -Path "$Drive\cubegm\cores\filelist.xml" -Pattern "LGPT|lgpt" -ErrorAction SilentlyContinue | ForEach-Object { $_.Line }
Write-Host ""
Write-Host "Allfiles:"
Select-String -Path "$Drive\cubegm\allfiles.lst" -Pattern "LGPT|lgpt" -ErrorAction SilentlyContinue | ForEach-Object { $_.Line }
