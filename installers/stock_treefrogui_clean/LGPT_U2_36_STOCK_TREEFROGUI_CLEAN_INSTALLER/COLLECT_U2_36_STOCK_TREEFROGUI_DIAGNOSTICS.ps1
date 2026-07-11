param([string]$DriveLetter = "F")
$ErrorActionPreference = "Continue"
if (!$DriveLetter.EndsWith(":")) { $Drive = "$DriveLetter`:" } else { $Drive = $DriveLetter }
$Root = "$Drive\"
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$out = Join-Path (Get-Location) "LGPT_U2_36_STOCK_TREEFROGUI_DIAGNOSTICS_$stamp"
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Copy-IfExists($src, $name) {
  if (Test-Path -LiteralPath $src) {
    Copy-Item -LiteralPath $src -Destination (Join-Path $out $name) -Force
    Write-Host "COPIED $src"
  } else {
    Write-Host "MISSING $src"
  }
}

Copy-IfExists (Join-Path $Root "log.txt") "root_log.txt"
Copy-IfExists (Join-Path $Root "picoarch_init.log") "picoarch_init.log"
Copy-IfExists (Join-Path $Root "tfhijack.log") "tfhijack.log"
Copy-IfExists (Join-Path $Root "lgpt\lgpt_launcher.log") "lgpt_launcher.log"
Copy-IfExists (Join-Path $Root "lgpt\lgpt.log") "lgpt.log"
Copy-IfExists (Join-Path $Root "cubegm\lgpt") "cubegm_lgpt_handler.txt"
Copy-IfExists (Join-Path $Root "roms\lgpt\start.lgpt") "start.lgpt.txt"
Copy-IfExists (Join-Path $Root "lgpt\config.xml") "config.xml"

Get-ChildItem -LiteralPath $Root -Force -ErrorAction SilentlyContinue |
  Select-Object Name,Mode,Length,LastWriteTime |
  Format-Table -AutoSize | Out-File -FilePath (Join-Path $out "root_listing.txt") -Encoding UTF8

Get-ChildItem -LiteralPath (Join-Path $Root "roms") -Force -ErrorAction SilentlyContinue |
  Select-Object Name,Mode,Length,LastWriteTime |
  Format-Table -AutoSize | Out-File -FilePath (Join-Path $out "roms_listing.txt") -Encoding UTF8

Get-ChildItem -LiteralPath (Join-Path $Root "lgpt") -Recurse -Force -ErrorAction SilentlyContinue |
  Select-Object FullName,Mode,Length,LastWriteTime |
  Format-Table -AutoSize | Out-File -FilePath (Join-Path $out "lgpt_tree.txt") -Encoding UTF8

Get-FileHash -LiteralPath (Join-Path $Root "cubegm\cores\lgpt_libretro.so") -Algorithm SHA256 -ErrorAction SilentlyContinue |
  Format-List | Out-File -FilePath (Join-Path $out "core_hash.txt") -Encoding UTF8

Write-Host "Diagnostics written to:"
Write-Host $out
