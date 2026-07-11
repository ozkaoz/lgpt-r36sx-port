# Comandos U2.23

```bash
SRC="/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT"
WORK="/mnt/d/R36S/PORT LPTRACKER"
SCRIPT="$WORK/APPLY_U2_23_SONG_YX_CUT_TAIL_CLEAR_FIX.sh"
chmod +x "$SCRIPT"
"$SCRIPT" "$SRC"
```

Después de compilar, copiar a SD y verificar hash:

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
