# Comandos WSL para build y copia a SD - U2.36 estable

## Build

```bash
SRC="/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT"
cd "$SRC"
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh
ls -lh "$SRC/dist/lgpt_libretro.so"
sha256sum "$SRC/dist/lgpt_libretro.so"
```

## Verificación de marcadores

```bash
SRC="/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT"
CPP="$SRC/sources/Application/Views/ModalDialogs/SampleChopperModal.cpp"
IMP="$SRC/sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp"
MGR="$SRC/sources/Application/Views/ModalDialogs/SampleManagerDialog.cpp"
POOL="$SRC/sources/Application/Instruments/SamplePool.cpp"
CORE="$SRC/dist/lgpt_libretro.so"

grep -n "TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE" "$CPP" "$IMP" "$MGR" "$POOL"
grep -n "Graphical Chopper U2.36" "$CPP"
grep -n "PITCH/ENV U2.36" "$CPP"
grep -n "FindIdenticalProjectSample" "$POOL"

ls -lh "$CORE"
file "$CORE"
sha256sum "$CORE"
```

## Copia a SD

Cambiar `F:` por la letra real de la SD.

```bash
SRC="/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT"

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "\
\$src = '$(wslpath -w "$SRC/dist/lgpt_libretro.so")'; \
\$dst = 'F:\cubegm\cores\lgpt_libretro.so'; \
if (!(Test-Path -LiteralPath \$src)) { throw \"No existe core local: \$src\" }; \
if (!(Test-Path -LiteralPath 'F:\cubegm\cores')) { throw 'No existe F:\cubegm\cores. Cambia F: por la letra real de la SD.' }; \
Copy-Item -LiteralPath \$src -Destination \$dst -Force; \
\$h1 = (Get-FileHash \$src -Algorithm SHA256).Hash; \
\$h2 = (Get-FileHash \$dst -Algorithm SHA256).Hash; \
Write-Host \"LOCAL_SHA256=\$h1\"; \
Write-Host \"SD_SHA256=\$h2\"; \
if (\$h1 -ne \$h2) { throw 'ERROR: hash diferente después de copiar' }; \
Get-Item \$dst | Format-List FullName,Length,LastWriteTime; \
Write-Host 'OK: core U2.36 copiado a SD y verificado.'"
```
