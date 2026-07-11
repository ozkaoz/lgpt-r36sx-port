# Comandos rápidos — versión estable con CHOP integrado

## Compilar en WSL Ubuntu 24

```bash
SRC="/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT"
cd "$SRC" || exit 1
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh
```

La compilación correcta debe terminar con un core en:

```bash
ls -lh "$SRC/dist/lgpt_libretro.so"
sha256sum "$SRC/dist/lgpt_libretro.so"
```

## Copiar a SD y verificar hash

Ajustar `F:` si la SD tiene otra letra.

```bash
SRC="/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT"
CORE_WIN="$(wslpath -w "$SRC/dist/lgpt_libretro.so")"

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "\
\$src = '$CORE_WIN'; \
\$dst = 'F:\cubegm\cores\lgpt_libretro.so'; \
if (!(Test-Path -LiteralPath \$src)) { throw \"No existe core local: \$src\" }; \
if (!(Test-Path -LiteralPath 'F:\cubegm\cores')) { throw 'No existe destino F:\cubegm\cores. Revisa la letra de la SD.' }; \
Copy-Item -LiteralPath \$src -Destination \$dst -Force; \
\$local = (Get-FileHash \$src -Algorithm SHA256).Hash; \
\$sd = (Get-FileHash \$dst -Algorithm SHA256).Hash; \
Write-Host \"LOCAL_SHA256=\$local\"; \
Write-Host \"SD_SHA256=\$sd\"; \
if (\$local -ne \$sd) { throw 'Hash mismatch after copy' }; \
Get-Item \$dst | Format-List FullName,Length,LastWriteTime; \
Write-Host 'OK: core copied and verified.'"
```
