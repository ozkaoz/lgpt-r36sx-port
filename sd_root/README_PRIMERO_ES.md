# LGPT R36SX Bacon 1.3 — General Review of the Port Architecture

Release de revisión general de la arquitectura del port. Mismo
comportamiento que Bacon 1.2.1 (golden) reorganizado internamente:
política única de input, menús y navegación, clases divididas por
responsabilidad, audio como servicios/backends extensibles, política
estricta de almacenamiento en la SD y deuda técnica eliminada — todo
verificado con pruebas de regresión y sin alterar sonido, controles,
timings, sampler, USB, proyectos ni compatibilidad.

ABI7, audio 48 kHz stereo. Cuatro modos de audio USB seleccionables
(Local / Windows / Android / Sampler SP404MKII), frontend-safe y sin
escrituras a la SD en runtime.

## Estado actual (U2.72 H43, 14/08/2026)

Build marcada como lista para release tras prueba en consola completa
(arranque, navegación, Mixer, Chopper con Pitch/Trim, undo/redo, USB-REC,
modos de audio). Core `ea7a80e4`, daemon `4be71632`.

## Qué incluye esta release (refactor F1-F10)

- **Input**: `Physical Input -> ChordResolver -> Semantic Action ->
  Context Policy -> Controller`. Bindings idénticos al golden (B=Preview,
  A=Apply, L1+A=menú, etc.) gobernados por un catálogo único.
- **Menús/Navegación**: NavigationController central (push/pop de modales
  seguros, sin use-after-free) y Help con la misma fuente de verdad que
  los controles.
- **Clases divididas**: Chopper (ChopModel, ChopperController, ChopperView,
  TrimTool, PitchEnvelopeTool, PreviewService, SampleEditHistory), Mixer
  (FxPages, MixerMeters, MixerMenu, FxNavigator) y Phrase
  (PhraseGridEdit, PhraseUndo).
- **Audio**: AudioEngine -> AudioRouter -> AudioBackend +
  AudioCapabilities (Local/Windows/Android/SP404/MIDI), preparado para
  capacidades futuras (multitrack) sin tocar la ruta estable.
- **Storage/SD**: política estricta — todo estado runtime en tmpfs;
  USB-REC graba en RAM/tmpfs y solo `Save` publica a la SD.
- **Riesgos y límites** del camino crítico de audio documentados (ASRC
  ±1200 ppm, backlog 2400 frames, re-enumeración 8 intentos).

## Bugs conocidos

- **Crash del Chopper (frío, one-shot)**: tras el arranque en frío, el
  primer edit destructivo puede crashear de forma puntual; no
  reproducible al repetir la operación. Sospecha: contención
  daemon/SD en frío, no regresión del refactor (mismo comportamiento que
  el golden). El crash del preview de pitch (rewrite del WAV compartido
  mientras el stream seguía activo) ya fue corregido (U2.52.0: Stop antes
  del rewrite).
- **Lag del driver Windows USB Audio**: lag pequeño e intermitente en el
  modo Windows UAC2, no reproducible en modo local ni en los otros
  modos. Hipótesis pendiente: backpressure/ASRC. Si lo reproduces,
  reporta con el log de `LGPT_OTG_LOGS`.

## Contenido del paquete

- `cubegm/cores/lgpt_r36sx_port_libretro.so` — core ABI7 (4 modos) Bacon 1.3.
- `cubegm/lgpt` — launcher del port.
- `lgpt/otg/bin/r36s_u241_usb_audio_io` — daemon Windows/origen UAC2.
- `lgpt/otg/bin/r36s_sp404_host_audio_io` — daemon SP404MKII (Sampler, UAC2 host).
- `lgpt/otg/bin/r36s_midi_host_io` — daemon USB-MIDI host.
- `lgpt/otg/bin/otg_*.sh` — selector de driver ABI7, supervisores y perfil UAC2.
- `lgpt/otg/bin/r36s_aoa_bulk_audio_io_h36` + `r36s_aoa_bulk_receiver_h36` — daemons Android h36.
- `lgpt/otg/modules/4.4.186-release/` — módulos kernel ALSA/UAC2 48k stereo
  (`host_usb_audio` y `u2_38au8_sync_uac2`).
- `ANDROID/LGPTUsbAudioBridge-H36-debug.apk` y
  `ANDROID/LGPTUsbAudioBridge-H38-debug.apk` — APK del puente de audio Android.
- `LGPT_OTG_LOGS/` — directorio de logs del runtime (vacío; todo tmpfs).
- `roms/lgpt/` — projects/samples de arranque.
- `lgpt/` — config, projects, samples, instruments (stock).

## Instalación en la SD (copiar carpetas a la raíz)

1. Partir de una SD con sistema Stock y TreeFrogUI (o conservar la actual).
2. Extraer el ZIP de la release.
3. Copiar las carpetas `cubegm`, `lgpt`, `roms`, `LGPT_OTG_LOGS` y
   `ANDROID` del ZIP directamente a la raíz de la SD, combinando
   carpetas y reemplazando archivos.
4. Instalar `ANDROID/LGPTUsbAudioBridge-H38-debug.apk` en el dispositivo
   Android (solo para el modo Android).
5. Expulsar la SD de forma segura.
6. Iniciar LGPT desde TreeFrogUI.

## Modos de audio

1. **Local** — salida local de la consola.
2. **Windows** — LGPT como adaptador USB-C audio-out para PC.
3. **Android** — puente de audio USB con el dispositivo Android vía APK.
4. **Sampler** — SP404MKII como sampler (EXT SOURCE), entrada USB-UAC2.

## Checksums

```text
CORE   (lgpt_r36sx_port_libretro.so)  ea7a80e473d9aa12f67feb075b6d171df641fc748a6f4a75330fb5c8703c4cfc
DAEMON (r36s_u241_usb_audio_io)       4be716329bbae1cc7f7b6a1de28a47aedfd18072e26b2a21678ce1b00075213e
SP404  (r36s_sp404_host_audio_io)     e3acc1f40a9142d0926a480df792cdcca11df4632885402b9c29ef1604ff9dcc
MIDI   (r36s_midi_host_io)            3f0ea7a23db7390f1fb3b73cbda97f66316c6568d0c7574b838579a014baee80
MODULE (usb_f_uac2.ko U2414)          e9062ac5a37aa7706c98a3411a57dbe67bc98ef82621c430c1d4e7a89b2fbefe
APK    (LGPTUsbAudioBridge-H38)       89a99d50571948788f1f0d2a3c1ebf313e6bf7a531874b5fc0885ba754ef1c3a
```
