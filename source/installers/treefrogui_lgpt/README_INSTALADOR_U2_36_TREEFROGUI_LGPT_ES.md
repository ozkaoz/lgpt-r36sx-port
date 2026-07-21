# Instalador LGPT U2.36 para TreeFrogUI 1.0.0

Este instalador corrige la instalación en SD para que el port arranque desde la carpeta **LGPT** nativa de TreeFrogUI, no desde GBA ni desde una carpeta suelta fuera de `roms`.

## Qué crea en la SD

Crea la carpeta visible de TreeFrogUI:

```text
roms/LGPT/LGPT_U2_36.lgpt
roms/LGPT/filelist.csv
```

Crea la carpeta de datos en la raíz de la SD:

```text
LGPT/config.xml
LGPT/projects/
LGPT/samples/
LGPT/instruments/
LGPT/images/
LGPT/exports/
LGPT/chops/
LGPT/tmp/
LGPT/backups/
```

Copia o valida el core:

```text
cubegm/cores/lgpt_libretro.so
```

Y registra el launcher contra ese core en:

```text
cubegm/cores/filelist.xml
cubegm/allfiles.lst
```

## Uso desde WSL

Primero asegúrate de haber compilado U2.36 y de tener:

```text
/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT/dist/lgpt_libretro.so
```

Luego ejecuta, ajustando la letra si tu SD no es `F:`:

```bash
bash INSTALL_U2_36_LGPT_TREEFROGUI_FROM_WSL.sh F /home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT/dist/lgpt_libretro.so
```

## Uso desde PowerShell de Windows

```powershell
powershell -ExecutionPolicy Bypass -File .\INSTALL_U2_36_LGPT_TREEFROGUI.ps1 -Drive F: -CorePath C:\ruta\lgpt_libretro.so
```

## Prueba en la consola

En TreeFrogUI entra por:

```text
LGPT -> LGPT U2.36
```

Dentro de LGPT valida:

```text
Chopper: Graphical Chopper U2.36
L1+R1:  PITCH/ENV U2.36
Instrument -> sample -> A: Listen Import Manage Exit
```

Si no aparece la carpeta LGPT, ejecuta:

```powershell
powershell -ExecutionPolicy Bypass -File .\VERIFY_U2_36_TREEFROGUI_SD.ps1 -Drive F:
```
