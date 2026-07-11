# Continuar desarrollo desde U2.41

Este paquete debe tratarse como el punto de continuación estable para el port LGPT R36SX con exportación WAV.

## Regla base

No compilar dentro de una ruta con espacios, por ejemplo:

```text
/mnt/d/R36S/PORT LPTRACKER/...
```

Esa ruta puede romper `make` por el espacio en `PORT LPTRACKER`. Compilar siempre desde `$HOME`.

## Preparar entorno de trabajo en WSL

Desde Windows, guarda este ZIP en:

```text
D:\R36S\PORT LPTRACKER\
```

En WSL:

```bash
cd ~
rm -rf lgpt_u241_dev
mkdir -p lgpt_u241_dev
cd lgpt_u241_dev
unzip -q "/mnt/d/R36S/PORT LPTRACKER/LGPT_PORT_U2_41_FINAL_WAV_EXPORT_SOURCE.zip"
cd LGPT_PORT_U2_41_FINAL_WAV_EXPORT_SOURCE
```

## Verificar fuente

```bash
bash VERIFY_U2_36_FINAL_PRE_OTG_SOURCE.sh
```

Este script valida presencia de archivos críticos del port. Puede mostrar nombres U2.36 porque la base histórica del port conserva esa nomenclatura.

## Compilar core libretro para TreeFrog/R36SX

```bash
bash BUILD_U2_36_STABLE_WSL.sh
```

Salida esperada:

```text
dist/lgpt_libretro.so
```

Comprobar:

```bash
ls -lh dist/lgpt_libretro.so
sha256sum dist/lgpt_libretro.so
```

## Copiar core a carpeta de trabajo en Windows

```bash
cp -f dist/lgpt_libretro.so "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_41_WAV.so"
```

## Instalar en SD

Ajusta `F` a la letra real de la SD.

```bash
bash INSTALL_U2_36_LGPT_TREEFROGUI_FROM_WSL.sh F "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_41_WAV.so"
```

## Verificación post-instalación

```bash
find /mnt/f/roms -iname "*.lgpt" -print
find /mnt/f/lgpt/exports -iname "*.wav" -print
cat /mnt/f/lgpt/wav_export_debug.log
```

Debe existir una sola entrada visible:

```text
/mnt/f/roms/lgpt/start.lgpt
```

## Prueba funcional mínima

1. Iniciar el port desde `start.lgpt`.
2. Abrir un proyecto con contenido en Song.
3. Ir a `Project > Export > Song WAV`.
4. Pulsar `START`.
5. Esperar la pantalla `Exporting song WAV` hasta finalizar.
6. Verificar el WAV en `F:\lgpt\exports\<ProjectName>\`.
7. Repetir con `Project > Export > Multitrack`.
8. Confirmar que solo se escriben stems con instrumento explícito usado en Song.

## Hotspots de código

### Exportación WAV

```text
sources/Application/Mixer/MixerService.cpp
sources/Application/Mixer/MixerService.h
```

Contiene:

- estado de job no bloqueante;
- pantalla de progreso;
- cálculo de ruta de exportación;
- naming por proyecto/instrumento;
- filtrado de canales sin instrumento explícito;
- escritura de Song WAV y Multitrack.

### Avance offline del reproductor

```text
sources/Application/Player/Player.cpp
```

Contiene soporte para consolidar el contenido de Song sin depender de escucha en tiempo real.

### Escritura WAV y calidad

```text
sources/Application/Instruments/WavFileWriter.cpp
sources/Application/Instruments/WavFileWriter.h
```

Contiene:

- cierre de header WAV;
- conversión a 16-bit;
- margen/headroom;
- dither TPDF determinístico;
- protección de escritura.

### Runtime TreeFrog/libretro

```text
sources/Adapters/TREEFROG/
projects/Makefile.TREEFROG
BUILD_U2_36_STABLE_WSL.sh
```

### Instaladores

```text
INSTALL_U2_36_LGPT_TREEFROGUI_FROM_WSL.sh
INSTALL_U2_36_LGPT_TREEFROGUI.ps1
installers/treefrogui_lgpt/
```

Mantienen una sola entrada: `roms/lgpt/start.lgpt`.

## Convenciones para siguientes versiones

- Mantener textos visibles de UI en inglés.
- Mantener documentación de trabajo en español si el desarrollo sigue en este flujo.
- No reintroducir launchers alternativos.
- No usar `project:exports` para WAV final; la ruta estable es `/mnt/sdcard/lgpt/exports`.
- Toda exportación debe cerrar el writer WAV aun si se cancela.
- Evitar operaciones largas dentro del handler directo de input; usar jobs no bloqueantes.

## Próximos puntos posibles

- Normalización opcional offline por peak/RMS.
- Exportación 24-bit o 32-bit float como opción avanzada.
- Metadatos WAV/BWF básicos.
- Opción de exportar rango parcial del Song.
- Mejor diagnóstico visual si un stem se omite por falta de instrumento explícito.
