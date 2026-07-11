# LGPT U2.36 — instalador limpio para SD Stock + TreeFrogUI

Este instalador corrige los intentos anteriores y vuelve a la ruta que ya fue validada en hardware para TreeFrogUI 1.0.0:

```text
F:\roms\lgpt\start.lgpt
F:\cubegm\lgpt
F:\cubegm\cores\lgpt_libretro.so
F:\lgpt
```

Puntos importantes:

- Usa carpeta `roms\lgpt` en minúscula, no `roms\LGPT`.
- Usa runtime `lgpt` en minúscula, no `LGPT`.
- Usa launcher `start.lgpt`, no `LGPT_U2_36.md`.
- Instala `cubegm\lgpt` como handler shell validado. No usa wrapper ELF experimental.
- No añade overrides `LGPT_U2_36` a `cubegm\cores\filelist.xml` ni a `cubegm\allfiles.lst`.
- Preserva proyectos existentes bajo `F:\lgpt\projects`.
- Copia el core U2.36 a `F:\cubegm\cores\lgpt_libretro.so` y también a `F:\cubegm\lgpt_libretro.so` para cubrir handlers stock alternativos.

## Uso desde WSL

Copia este ZIP a:

```text
D:\R36S\PORT LPTRACKER
```

Luego ejecuta:

```bash
cd "/mnt/d/R36S/PORT LPTRACKER"
rm -rf LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER
unzip -o LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER.zip -d .

bash LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER/INSTALL_U2_36_STOCK_TREEFROGUI_FROM_WSL.sh F "/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT/dist/lgpt_libretro.so"

bash LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER/VERIFY_U2_36_STOCK_TREEFROGUI_FROM_WSL.sh F
```

## Prueba en consola

Expulsa la SD desde Windows, arranca la R36S y entra por:

```text
LGPT -> start.lgpt
```

El port debe crear este log al ejecutarse el handler:

```text
F:\lgpt\lgpt_launcher.log
```

Dentro de LGPT valida:

```text
Graphical Chopper U2.36
PITCH/ENV U2.36
Instrument -> sample -> A: Listen Import Manage Exit
```

## Proyecto default / Boom Bap

Este instalador no fabrica un proyecto Boom Bap si la SD ya no lo contiene. En una SD Stock + TreeFrogUI, el runtime stock de LGPT debe estar bajo `F:\lgpt`; este instalador lo preserva. Si `F:\lgpt\projects` no contiene ningún `lgptsav.dat`, la verificación lo reportará como advertencia.

Si falta el proyecto default, restaura primero la SD Stock + TreeFrogUI o copia el proyecto validado dentro de:

```text
F:\lgpt\projects\<nombre_del_proyecto>\lgptsav.dat
```

## Diagnóstico

Si parpadea y vuelve al menú, reinsertar la SD y ejecutar:

```bash
bash LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER/VERIFY_U2_36_STOCK_TREEFROGUI_FROM_WSL.sh F
bash LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER/COLLECT_U2_36_STOCK_TREEFROGUI_DIAGNOSTICS_FROM_WSL.sh F
```

Si `F:\lgpt\lgpt_launcher.log` no existe, TreeFrogUI no ejecutó `F:\cubegm\lgpt`. En ese caso el problema está en la carpeta/launcher visible. Si el log existe, el handler se ejecutó y el error está en `picoarch`, el core o el runtime.
