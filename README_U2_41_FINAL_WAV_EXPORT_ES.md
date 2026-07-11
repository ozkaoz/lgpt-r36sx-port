# LGPT R36SX U2.41 - Fuente final con exportación WAV

Estado: **estable de trabajo** para R36SX / TreeFrogUI, basado en U2.40 y consolidado como U2.41.

Esta versión mantiene la entrada única por `start.lgpt`, conserva el port estable con Chopper integrado y agrega exportación WAV no bloqueante con nombres de proyecto e instrumentos.

## Cambios funcionales de U2.41

- Menú en inglés: `Project > Export > Off / Song WAV / Multitrack`.
- Pantalla de progreso no bloqueante durante exportación.
- Exportación de canción completa a WAV sin tener que escuchar el proyecto en tiempo real.
- Exportación multitrack/stems con archivos separados por track usado.
- Los WAV se agrupan por proyecto:

```text
F:\lgpt\exports\<ProjectName>\
```

- Song WAV:

```text
F:\lgpt\exports\<ProjectName>\<ProjectName>.wav
F:\lgpt\exports\<ProjectName>\<ProjectName>_001.wav
```

- Multitrack:

```text
F:\lgpt\exports\<ProjectName>\multitrack\<ProjectName>_<InstrumentName>_track_01.wav
```

- Los canales sin instrumento explícitamente asignado en Song no se exportan.
- Los instrumentos sample vacíos se omiten; instrumentos MIDI siguen siendo stems válidos.
- Si un track usa más de un instrumento explícito, se usa el primer instrumento para el nombre y se añade `_multi`.

## Entrada única al port

La única entrada visible esperada en TreeFrogUI es:

```text
F:\roms\lgpt\start.lgpt
```

En WSL:

```text
/mnt/f/roms/lgpt/start.lgpt
```

El instalador limpia entradas antiguas en `roms/LGPT`, `roms/lgpt`, `GBA` y rutas previas del port.

## Rutas de runtime en la SD

```text
F:\lgpt\config.xml
F:\lgpt\projects\
F:\lgpt\samples\
F:\lgpt\instruments\
F:\lgpt\exports\
F:\lgpt\wav_export_debug.log
F:\roms\lgpt\start.lgpt
```

Equivalente en la consola/WSL:

```text
/mnt/sdcard/lgpt/config.xml
/mnt/sdcard/lgpt/projects/
/mnt/sdcard/lgpt/samples/
/mnt/sdcard/lgpt/instruments/
/mnt/sdcard/lgpt/exports/
/mnt/sdcard/lgpt/wav_export_debug.log
/mnt/f/roms/lgpt/start.lgpt
```

## Archivos principales modificados para exportación

```text
sources/Application/Mixer/MixerService.cpp
sources/Application/Mixer/MixerService.h
sources/Application/Player/Player.cpp
sources/Application/Instruments/WavFileWriter.cpp
sources/Application/Instruments/WavFileWriter.h
INSTALL_U2_36_LGPT_TREEFROGUI_FROM_WSL.sh
INSTALL_U2_36_LGPT_TREEFROGUI.ps1
```

Los scripts conservan nombres U2.36 por compatibilidad histórica del port, pero la fuente incluida en este paquete corresponde a U2.41.
