# LGPT R36SX Bacon 1.2 — Mixer (release final)

Versión final Bacon 1.2, rama `stabilize-bacon-1.2.1`, ABI7, audio 48 kHz
stereo. Cuatro modos de audio USB seleccionables (Local / Windows / Android /
Sampler SP404MKII), frontend-safe y sin escrituras a la SD en runtime.

## Estado actual (U2.72 H43, 13/08/2026)

Prueba de campo completa superada: switches directos repetidos entre los
cuatro modos y sesiones continuas de Sampler y Windows sin crash, sin stall y
sin el detour manual por Android. El port queda estable para uso continuado.

## Cambios aplicados en esta release (U2.70 → U2.72)

- **U2.70** — ASRC FIR16 Lanczos-8 en ambos daemons (tabla limpia, fila
  79==81 corregida) + selector `ASRC_FIR_TAPS` A/B.
- **U2.71 H39+H40/H41/H42** — presupuesto wall-clock de la SD, staging de
  escrituras parciales (cero escrituras a la SD en runtime), micro-ASRC PI en
  el daemon Windows y fixes del ASRC de Sampler.
- **U2.72 H43** —
  - Core: `signal(SIGPIPE, SIG_IGN)` — el core moría al entrar a Sampler
    cuando el daemon SP404 (único lector del FIFO) se eliminaba en el cambio
    de modo.
  - Bridge: el fast-apply del cambio de driver solo aplica cuando el FIFO
    abierto es el correcto; si no, cierra el FIFO y fuerza el apply completo.
    Elimina el bloqueo SP404→Windows que exigía el detour por Android.
  - Daemon SP404: tope de corrección ASRC ±1.200 ppm (elimina el pitch
    audible de ±1 %), EMA del backlog de control, hold floor de 2.400 frames
    (nunca drena el ring) y salida del daemon tras 8 REENUM fallidos
    (`SP404_REENUM_EXHAUSTED`) que libera el rol musb host.
  - El pitch residual del modo Sampler (~+0,3 %) es inherente al reloj
    ADAPTIVE del SP404 y queda compensado por el ASRC (inaudible).

## Contenido del paquete

- `cubegm/cores/lgpt_r36sx_port_libretro.so` — core ABI7 (4 modos) U2.72 H43.
- `lgpt/otg/bin/r36s_u241_usb_audio_io` — daemon Windows/origen UAC2.
- `lgpt/otg/bin/r36s_sp404_host_audio_io` — daemon SP404MKII (Sampler, UAC2 host).
- `lgpt/otg/bin/r36s_midi_host_io` — daemon USB-MIDI host.
- `lgpt/otg/bin/otg_h37_apply_driver_mode.sh` — selector de driver ABI7.
- `lgpt/otg/bin/otg_h37_host_runtime_supervisor.sh` — supervisor host (SP404/Windows).
- `lgpt/otg/bin/otg_h37_android_runtime_supervisor.sh` — supervisor Android h36.
- `lgpt/otg/bin/otg_h37_host_device_detect.sh` — detección de dispositivos host.
- `lgpt/otg/bin/otg_u241_apply_profile_once.sh` / `otg_u241_common.sh` /
  `otg_u241_setup_once.sh` / `otg_u241_shutdown.sh` — gestión del perfil UAC2.
- `lgpt/otg/bin/r36s_aoa_bulk_audio_io_h36` + `r36s_aoa_bulk_receiver_h36` — daemons Android h36.
- `lgpt/otg/modules/4.4.186-release/` — módulos kernel ALSA/UAC2 48k stereo
  (`host_usb_audio` y `u2_38au8_sync_uac2`).
- `ANDROID/LGPTUsbAudioBridge-H36-debug.apk` y
  `ANDROID/LGPTUsbAudioBridge-H38-debug.apk` — APK del puente de audio Android.
- `SOURCE_AND_TOOLS/full_repository/` — snapshot completo del repositorio en
  el commit publicado (no copiar a la SD).

## Instalación en otra SD (solo copiar carpetas)

1. Partir de una SD con sistema Stock y TreeFrogUI.
2. Extraer el ZIP de la release.
3. Copiar las carpetas `cubegm`, `lgpt`, `roms`, `LGPT_OTG_LOGS` y `ANDROID`
   del ZIP directamente a la raíz de la SD, combinando carpetas y reemplazando
   archivos.
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
CORE   (lgpt_r36sx_port_libretro.so)  771c3844161269cabecb3de2de9ab25509eff3ef9cca1f5701960e9cb6a13f4a
SP404  (r36s_sp404_host_audio_io)     0c74c1400b98e455990f29c5f1f61c21f70e8d7f1fc74edca32b76b412572548
DAEMON (r36s_u241_usb_audio_io)       20c8aea6b1b2191fd89505de18e20330dea70d67964379f4d26a6b746210e0b9
MIDI   (r36s_midi_host_io)            3f0ea7a23db7390f1fb3b73cbda97f66316c6568d0c7574b838579a014baee80
APK    (LGPTUsbAudioBridge-H38)       89a99d50571948788f1f0d2a3c1ebf313e6bf7a531874b5fc0885ba754ef1c3a
```