param(
  [string]$DriveLetter = "F",
  [Parameter(Mandatory=$true)][string]$CorePath,
  [Parameter(Mandatory=$true)][string]$ConfigPath,
  [Parameter(Mandatory=$true)][string]$HandlerPath
)

$ErrorActionPreference = "Stop"

function Normalize-Drive([string]$d) {
  $d = $d.Trim()
  if ($d.EndsWith(":")) { return $d }
  return "$d`:" 
}

function Ensure-Dir-Case([string]$Parent, [string]$Name) {
  if (!(Test-Path -LiteralPath $Parent)) {
    New-Item -ItemType Directory -Force -Path $Parent | Out-Null
  }
  $target = Join-Path $Parent $Name
  $existing = Get-ChildItem -LiteralPath $Parent -Force -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ieq $Name } |
    Select-Object -First 1

  if ($null -eq $existing) {
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    return $target
  }

  if ($existing.Name -cne $Name) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $tmp = Join-Path $Parent ("__lgpt_casefix_${Name}_$stamp")
    Move-Item -LiteralPath $existing.FullName -Destination $tmp -Force
    Move-Item -LiteralPath $tmp -Destination $target -Force
    return $target
  }

  return $existing.FullName
}

function Write-AsciiNoBom([string]$Path, [string]$Text) {
  $enc = New-Object System.Text.ASCIIEncoding
  [System.IO.File]::WriteAllText($Path, $Text, $enc)
}

function Write-Utf8NoBom([string]$Path, [string]$Text) {
  $enc = New-Object System.Text.UTF8Encoding($false)
  [System.IO.File]::WriteAllText($Path, $Text, $enc)
}

function Remove-IfExists([string]$Path) {
  if (Test-Path -LiteralPath $Path) {
    Remove-Item -LiteralPath $Path -Force -Recurse -ErrorAction SilentlyContinue
  }
}

$Drive = Normalize-Drive $DriveLetter
$Root = "$Drive\"
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"

Write-Host "== LGPT U2.36 STOCK + TREEFROGUI CLEAN INSTALL =="
Write-Host "Drive: $Drive"
Write-Host "Core:  $CorePath"
Write-Host ""

if (!(Test-Path -LiteralPath $Root)) { throw "No existe la unidad $Drive" }
if (!(Test-Path -LiteralPath (Join-Path $Root "cubegm"))) { throw "No existe $Drive\cubegm. La SD no parece Stock + TreeFrogUI." }
if (!(Test-Path -LiteralPath (Join-Path $Root "roms"))) { throw "No existe $Drive\roms. La SD no parece Stock + TreeFrogUI." }
if (!(Test-Path -LiteralPath (Join-Path $Root "frogui"))) { Write-Host "WARN: no existe $Drive\frogui. Continuo, pero revisa que TreeFrogUI esté instalado." }
if (!(Test-Path -LiteralPath $CorePath)) { throw "No existe el core local: $CorePath" }
if (!(Test-Path -LiteralPath $ConfigPath)) { throw "No existe config.xml del instalador: $ConfigPath" }
if (!(Test-Path -LiteralPath $HandlerPath)) { throw "No existe handler del instalador: $HandlerPath" }

$cubegm = Ensure-Dir-Case $Root "cubegm"
$cores = Ensure-Dir-Case $cubegm "cores"
$roms = Ensure-Dir-Case $Root "roms"
$romsLgpt = Ensure-Dir-Case $roms "lgpt"
$lgptRoot = Ensure-Dir-Case $Root "lgpt"

foreach ($sub in @("projects","samples","instruments","images","exports","chops","tmp","backups")) {
  Ensure-Dir-Case $lgptRoot $sub | Out-Null
}

Write-Host "== CLEAN WRONG U2 LAUNCHER ARTIFACTS =="
foreach ($bad in @(
  (Join-Path $romsLgpt "LGPT_U2_36.md"),
  (Join-Path $romsLgpt "LGPT_U2_36.lgpt"),
  (Join-Path $romsLgpt "filelist.csv"),
  (Join-Path $lgptRoot "LGPT_U2_36.md"),
  (Join-Path $lgptRoot "LGPT_U2_36.lgpt"),
  (Join-Path $lgptRoot "filelist.csv"),
  (Join-Path $lgptRoot "lgpt_u236_launch.log")
)) { Remove-IfExists $bad }

# Clean stale explicit overrides from previous experimental installers.
$coreFilelist = Join-Path $cores "filelist.xml"
if (Test-Path -LiteralPath $coreFilelist) {
  Copy-Item -LiteralPath $coreFilelist -Destination "$coreFilelist.bak_u236_clean_$stamp" -Force
  $xml = Get-Content -LiteralPath $coreFilelist -Raw -ErrorAction SilentlyContinue
  $xml = ($xml -split "`r?`n") | Where-Object { $_ -notmatch "LGPT_U2_36|roms/LGPT|roms/lgpt/LGPT_U2|LGPT/LGPT_U2" }
  Set-Content -LiteralPath $coreFilelist -Value ($xml -join "`r`n") -Encoding ASCII -Force
}

$allfiles = Join-Path $cubegm "allfiles.lst"
if (Test-Path -LiteralPath $allfiles) {
  Copy-Item -LiteralPath $allfiles -Destination "$allfiles.bak_u236_clean_$stamp" -Force
  $lines = Get-Content -LiteralPath $allfiles -ErrorAction SilentlyContinue |
    Where-Object { $_ -notmatch "LGPT_U2_36|roms/LGPT|LGPT/LGPT_U2" }
  Set-Content -LiteralPath $allfiles -Value $lines -Encoding ASCII -Force
}

Write-Host "== INSTALL VALIDATED LOWERCASE TREEFROGUI ROUTE =="
$start = Join-Path $romsLgpt "Start.lgpt"
Write-AsciiNoBom $start "LGPT_START`n"

$configDest = Join-Path $lgptRoot "config.xml"
Copy-Item -LiteralPath $ConfigPath -Destination $configDest -Force

$coreDest1 = Join-Path $cores "lgpt_libretro.so"
$coreDest2 = Join-Path $cubegm "lgpt_libretro.so"
Copy-Item -LiteralPath $CorePath -Destination $coreDest1 -Force
Copy-Item -LiteralPath $CorePath -Destination $coreDest2 -Force

$handlerDest = Join-Path $cubegm "lgpt"
if (Test-Path -LiteralPath $handlerDest) {
  Copy-Item -LiteralPath $handlerDest -Destination "$handlerDest.bak_u236_clean_$stamp" -Force
}
$handlerText = Get-Content -LiteralPath $HandlerPath -Raw
$handlerText = $handlerText -replace "`r`n", "`n"
Write-Utf8NoBom $handlerDest $handlerText

# Keep a mirror name for users/tools that inspect .elf, but the active validated handler is cubegm\lgpt.
Copy-Item -LiteralPath $handlerDest -Destination (Join-Path $cubegm "lgpt.elf") -Force

# Optional gme placeholder: older experimental packages used gme incorrectly. Keep it non-executable only.
$romsGme = Ensure-Dir-Case $roms "gme"
Write-AsciiNoBom (Join-Path $romsGme "README_NO_EXECUTABLE_LGPT_ENTRY.txt") "Do not launch LGPT from gme. Use roms/lgpt/Start.lgpt via the LGPT platform.`n"

Write-Host "== PRESERVE / REPORT DEFAULT PROJECTS =="
$projects = Join-Path $lgptRoot "projects"
$saveFiles = @(Get-ChildItem -LiteralPath $projects -Recurse -Force -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -ieq "lgptsav.dat" })
if ($saveFiles.Count -eq 0) {
  $warn = @"
No lgptsav.dat project was found after installation.
This installer does not fabricate the Boom Bap/default project; it preserves it when it exists on a Stock + TreeFrogUI SD.
If you restored TreeFrogUI stock and the default project is expected, verify that it exists under F:\lgpt\projects before running destructive cleanup.
"@
  Write-AsciiNoBom (Join-Path $projects "README_DEFAULT_PROJECT_MISSING.txt") $warn
  Write-Host "WARN: No hay proyectos LGPT en $projects. Se creó README_DEFAULT_PROJECT_MISSING.txt"
} else {
  Write-Host "OK: proyectos LGPT detectados:"
  $saveFiles | Select-Object FullName,Length,LastWriteTime | Format-Table -AutoSize
}

Write-Host "== HASH CHECK =="
$localHash = (Get-FileHash -LiteralPath $CorePath -Algorithm SHA256).Hash
$sdHash1 = (Get-FileHash -LiteralPath $coreDest1 -Algorithm SHA256).Hash
$sdHash2 = (Get-FileHash -LiteralPath $coreDest2 -Algorithm SHA256).Hash
Write-Host "LOCAL_SHA256=$localHash"
Write-Host "SD_CORES_SHA256=$sdHash1"
Write-Host "SD_ROOTCOPY_SHA256=$sdHash2"
if ($localHash -ne $sdHash1 -or $localHash -ne $sdHash2) { throw "SHA256 mismatch copiando core" }

Write-Host ""
Write-Host "== OK: INSTALACION COMPLETADA =="
Write-Host "Ruta validada: $Drive\roms\lgpt\Start.lgpt -> $Drive\cubegm\lgpt -> $Drive\cubegm\cores\lgpt_libretro.so -> $Drive\lgpt"
Write-Host "Al ejecutar en consola debe crearse: $Drive\lgpt\lgpt_launcher.log"
