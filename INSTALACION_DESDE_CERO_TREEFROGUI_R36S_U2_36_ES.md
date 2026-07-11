# Instalación desde cero tras formatear SD y reinstalar TreeFrogUI

Este documento asume que se formateará la SD y se reinstalará TreeFrogUI. La compilación se realiza en WSL Ubuntu 24.

## 1. Restaurar TreeFrogUI en la SD

Después de reinstalar TreeFrogUI, verificar en Windows que existe una ruta equivalente a:

```text
F:\cubegm\cores
```

La letra `F:` puede ser distinta. Confirmarla desde WSL con:

```bash
powershell.exe -NoProfile -Command "Get-Volume | Sort-Object DriveLetter | Format-Table DriveLetter,FileSystemLabel,FileSystem,SizeRemaining,Size -AutoSize"
```

Verificar la ruta de cores, cambiando `F:` si corresponde:

```bash
powershell.exe -NoProfile -Command "Test-Path 'F:\cubegm\cores'"
```

Debe devolver `True`.

## 2. Colocar la fuente U2.36 en WSL

Ruta recomendada:

```bash
mkdir -p /home/dafunknoise/r36sx-lgpt-port/dev_sources
cp -a "/mnt/d/R36S/PORT LPTRACKER/LGPT_PORT_U2_36_ESTABLE_INDEPENDIENTE" \
  /home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT
```

Ajustar la ruta origen si el ZIP se extrae en otro lugar.

## 3. Compilar

```bash
SRC="/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT"
cd "$SRC"
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh
```

Debe generarse:

```bash
ls -lh "$SRC/dist/lgpt_libretro.so"
sha256sum "$SRC/dist/lgpt_libretro.so"
```

## 4. Copiar core a SD

Cambiar `F:` si la SD tiene otra letra:

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

## 5. Primera prueba tras reinstalar TreeFrogUI

En R36S:

1. Arrancar TreeFrogUI.
2. Abrir LGPT.
3. Cargar proyecto o crear uno nuevo.
4. Entrar a Chopper y verificar `Graphical Chopper U2.36`.
5. Entrar a `L1+R1` y verificar `PITCH/ENV U2.36`.
6. Ejecutar el protocolo `TEST_PROTOCOL_ESTABLE_U2_36_FINAL_ES.md`.
