param([string]$DriveLetter = "F")
$ErrorActionPreference = "Continue"
if (!$DriveLetter.EndsWith(":")) { $Drive = "$DriveLetter`:" } else { $Drive = $DriveLetter }
$Root = "$Drive\"

Write-Host "== VERIFY LGPT U2.36 STOCK TREEFROGUI LAYOUT =="
Write-Host "Drive: $Drive"
Write-Host ""

$required = @(
  "cubegm",
  "cubegm\picoarch",
  "cubegm\lgpt",
  "cubegm\cores",
  "cubegm\cores\lgpt_libretro.so",
  "cubegm\lgpt_libretro.so",
  "roms",
  "roms\lgpt",
  "roms\lgpt\start.lgpt",
  "lgpt",
  "lgpt\config.xml",
  "lgpt\projects",
  "lgpt\samples",
  "lgpt\instruments"
)
foreach ($p in $required) {
  $full = Join-Path $Root $p
  if (Test-Path -LiteralPath $full) { Write-Host "OK   $full" } else { Write-Host "MISS $full" }
}

Write-Host ""
Write-Host "== ACTUAL DIRECTORY CASE UNDER ROOT =="
Get-ChildItem -LiteralPath $Root -Force -Directory -ErrorAction SilentlyContinue |
  Where-Object { $_.Name -match '^(lgpt|roms|cubegm)$' } |
  Select-Object Name,FullName,LastWriteTime | Format-Table -AutoSize
Write-Host "== ACTUAL DIRECTORY CASE UNDER roms =="
$roms = Join-Path $Root "roms"
if (Test-Path -LiteralPath $roms) {
  Get-ChildItem -LiteralPath $roms -Force -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ieq 'lgpt' -or $_.Name -ieq 'gme' } |
    Select-Object Name,FullName,LastWriteTime | Format-Table -AutoSize
}

Write-Host ""
Write-Host "== HANDLER PREVIEW =="
Get-Content -LiteralPath (Join-Path $Root "cubegm\lgpt") -TotalCount 60 -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "== CORE HASHES =="
foreach ($p in @("cubegm\cores\lgpt_libretro.so", "cubegm\lgpt_libretro.so")) {
  $full = Join-Path $Root $p
  if (Test-Path -LiteralPath $full) { Get-FileHash -LiteralPath $full -Algorithm SHA256 }
}

Write-Host ""
Write-Host "== LGPT PROJECTS =="
$projects = Join-Path $Root "lgpt\projects"
if (Test-Path -LiteralPath $projects) {
  Get-ChildItem -LiteralPath $projects -Force -ErrorAction SilentlyContinue | Select-Object Name,Mode,Length,LastWriteTime | Format-Table -AutoSize
  $saves = @(Get-ChildItem -LiteralPath $projects -Recurse -Force -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -ieq "lgptsav.dat" })
  if ($saves.Count -gt 0) {
    Write-Host "OK: lgptsav.dat detectado:"
    $saves | Select-Object FullName,Length,LastWriteTime | Format-Table -AutoSize
  } else {
    Write-Host "WARN: no hay lgptsav.dat. El proyecto Boom Bap/default no está instalado en esta SD."
  }
}

Write-Host ""
Write-Host "== WRONG EXPERIMENTAL ENTRIES SHOULD BE EMPTY =="
foreach ($p in @("cubegm\cores\filelist.xml", "cubegm\allfiles.lst")) {
  $full = Join-Path $Root $p
  if (Test-Path -LiteralPath $full) {
    Write-Host "-- $full"
    Select-String -LiteralPath $full -Pattern "LGPT_U2_36|roms/LGPT|LGPT/LGPT_U2" -ErrorAction SilentlyContinue
  }
}

Write-Host ""
$log = Join-Path $Root "lgpt\lgpt_launcher.log"
if (Test-Path -LiteralPath $log) {
  Write-Host "== LAUNCHER LOG EXISTS =="
  Get-Content -LiteralPath $log -Tail 80 -ErrorAction SilentlyContinue
} else {
  Write-Host "No existe todavía $log. Se crea cuando TreeFrogUI ejecuta cubegm\lgpt."
}
