# LGPT R36SX Bacon 1.5 — Synths nativos + multitrack (items 6-8)

Release de desarrollo sobre la arquitectura de Bacon 1.4 (ABI7, audio
48 kHz stereo, cuatro modos de audio USB Local / Windows / Android /
Sampler SP404MKII, frontend-safe y sin escrituras a la SD en runtime).

## Estado actual (items 6-8, 15/08/2026)

Core **compilado e instalado** en esta SD (`cubegm/cores/lgpt_r36sx_port_libretro.so`,
SHA256 `2288cb5c3a6eb8644402a49618edfe710f8f2ab823aa0f762f8b7c29e3981eb4`).
Build device: `DIAGNOSTIC_GATE=0_ERRORS_0_WARNINGS`, `BUILD_U2523_OK`; regresión
host `AUDIT_CLEAN_MAIN_U2523_OK`. Daemons y módulo UAC2 sin cambios (baseline
Bacon 1.4, hashes `f7140072`/`968dfa61`/`3f0ea7a2`/`e9062ac5`). Commits:
`a0da94f` (item 7), `ee62202` (item 8), `c59ebc6` (fix DIAGNOSTIC_GATE).

## Qué incluye esta release (items 6-8)

- **Item 6 — BassSynth nativo** (slots `0x90`-`0x9F`, tipo `IT_SYNTH`): mono por
  canal, osc PolyBLEP saw/square/tri/sine sin aliasing, subosc square, noise
  LFSR xorshift32, amp ADSR + filter ADSR por etapas, Filter V2 TPT SVF
  LP/HP/BP/NOTCH por canal a control rate, drive soft-clip, accent, LFO
  0-20 Hz (CUT/VOL/PIT), pan equal-power, EQ8, sends dry/delay/reverb, tabla +
  automatización. Comandos: VOLM/PAN/FCUT/FRES (0-255→0-100), PTCH (bend
  ±12 st), LEGA, DLYS/RVBS en vivo.
- **Item 7 — PianoSynth nativo** (slots `0xA0`-`0xAF`, tipo `IT_PIANO`): aditivo
  con tabla seno 256 (band-limited, sin aliasing), 4 voces por canal, modos EP
  y TINE (parciales armónicos), velocity, pitch bend, width Haas (~21 ms),
  Filter V2 por canal, sustains, re-strike y voice-stealing.
- **Item 8 — Multitrack / stems de export**: además de `channel0-7.wav` el
  render multipista escribe `project:delayret.wav`, `project:reveret.wav` y
  `project:master.wav`. Guard de espacio libre: el render aborta si quedan
  < 128 MB libres en la SD (`FileSystem::GetFreeSpace`, statvfs).

## Build del core (items 6-8)

En la máquina de compilación con la toolchain sf3000 (mipsel):

```bash
cd /mnt/d/R36S/PORT\ LPTRACKER/GITHUB/lgpt-r36sx-port
bash scripts/build.sh
```

El core queda en `BUILD/U2523/lgpt_r36sx_u2523.so`. Copiarlo a la SD:

```bash
cp BUILD/U2523/lgpt_r36sx_u2523.so sd_root/cubegm/cores/lgpt_r36sx_port_libretro.so
```

## Pruebas a hacer en consola (items 6-8)

1. **BassSynth**: crear instrumento en un canal, tipo SYNTH (slots 0x90-0x9F).
   Probar las 4 formas de onda (saw/square/tri/sine) sin aliasing en agudos;
   subosc, noise, glide/portamento (PTCH/LEGA), accent, LFO sobre CUT/VOL/PIT,
   drive, filtro LP/HP/BP/NOTCH, ADSR de amp y de filtro, pan, EQ8, sends.
2. **PianoSynth**: tipo PIANO (slots 0xA0-0xAF). Modos EP y TINE (parciales 1P
   a 4P), timbre, pdecay, width Haas, velocity, sustains (colas naturales),
   acordes de 4 notas (voice-stealing), filtro y sends.
3. **EQ8**: 8 bandas con la página EXT (5 bandas adicionales: frecuencia,
   ganancia, Q, tipo, bypass con crossfade) en Master y en instrumento.
4. **Sends**: dry 0-100 y delay/reverb -1..100 (0xFF hereda del track) en
   BassSynth y PianoSynth; hear return delay y reverb en el send master.
5. **Export multitrack**: render en modo MULTITRACK → `channel0-7.wav`,
   `delayret.wav`, `reveret.wav` y `master.wav`; comprobar longitudes iguales,
   que master = mezcla de canales + returns, y que el render aborta con aviso
   si la SD tiene < 128 MB libres.
6. **Regresión**: modos de audio (Local/Windows/Android/Sampler), Mixer,
   Chopper, persistencia de proyectos y arranque en frío.

## Bugs conocidos

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