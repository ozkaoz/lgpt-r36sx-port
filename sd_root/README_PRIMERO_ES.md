# LGPT R36SX Bacon 1.4 — Bug Fixes

Release de corrección de bugs sobre la arquitectura de Bacon 1.3.
Mismo comportamiento, sonido, controles, timings, sampler, USB,
proyectos y compatibilidad — sin cambios de arquitectura.

ABI7, audio 48 kHz stereo. Cuatro modos de audio USB seleccionables
(Local / Windows / Android / Sampler SP404MKII), frontend-safe y sin
escrituras a la SD en runtime.

## Estado actual (U2.54, 14/08/2026)

Build marcada como lista para release tras prueba en consola completa
(arranque, navegación, Mixer, Chopper con Pitch/Env, audio Windows y
modos de audio). Core `e4fbbdc8`, daemon `f7140072`.

## Qué corrige esta release

- **Audio Windows entrecortado**: corregido el lag/backpressure del
  driver Windows UAC2 (dos fixes: estabilidad del stream de salida y
  arranque del modo USB). Confirmado en consola con PC.
- **Panel Pitch/Env del Chopper**:
  - Ahora se muestra con el proyecto detenido (antes no aparecía).
  - Texto legible: la fuente TreeFrog es scanline-major con 1 byte por
    píxel y semántica ZERO_IS_INK; el render del panel usa la misma
    semántica que el resto de la UI.
  - Pantalla completa (0,0,320,240): tapa la cabecera del chopper
    (Graphical chopper, inst, sampl, zoom, name, frame, chop), el status
    y los hints del char screen; título, header, ítems, hints y status
    se redibujan dentro del panel, coherentes con el resto de los menús
    (fila editada invertida en HILITE2).
- **Crash del preview de pitch** (rewrite del WAV compartido mientras el
  stream seguía activo): corregido (Stop antes del rewrite, U2.52.0).

## Bugs conocidos

- **Crash del Chopper (frío, one-shot)**: tras el arranque en frío, el
  primer edit destructivo puede crashear de forma puntual; no
  reproducible al repetir la operación. Sospecha: contención
  daemon/SD en frío, heredado de Bacon 1.3.

## Contenido del paquete

- `cubegm/cores/lgpt_r36sx_port_libretro.so` — core ABI7 (4 modos) Bacon 1.4.
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
CORE   (lgpt_r36sx_port_libretro.so)  e4fbbdc82a487484cf6d9e3844427893b113e7dcbbdc3bb2edfccf4effd7e61d
DAEMON (r36s_u241_usb_audio_io)       f7140072f9b83573e03caf904d17de6227374823c3719757c7d11a438bb1417d
SP404  (r36s_sp404_host_audio_io)     968dfa61e561d348fd4ec8006e39b23b4dd56a49f1912f885c2731f118983b83
MIDI   (r36s_midi_host_io)            3f0ea7a23db7390f1fb3b73cbda97f66316c6568d0c7574b838579a014baee80
MODULE (usb_f_uac2.ko U2414)          e9062ac5a37aa7706c98a3411a57dbe67bc98ef82621c430c1d4e7a89b2fbefe
APK    (LGPTUsbAudioBridge-H38)       89a99d50571948788f1f0d2a3c1ebf313e6bf7a531874b5fc0885ba754ef1c3a
```