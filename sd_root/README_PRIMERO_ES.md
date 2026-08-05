# LGPT R36SX Bacon 1.1 — Audio Driver Refact, Sampler Audio Added

Versión Bacon 1.1, ABI7, cuatro drivers de audio (Local / Windows / Android /
Sampler), frontend-safe.

## Alcance

Este release consolida el refactor del driver de audio Sampler (SP404MKII) y
la estabilización completa de los cuatro drivers de audio USB seleccionables.
El pitido permanente del Sampler queda eliminado de raíz y el puerto se
verifica funcionando correctamente en los cuatro modos.

## Contenido

- `cubegm/cores/lgpt_r36sx_port_libretro.so` — core ABI7 (4 modos).
- `lgpt/otg/bin/r36s_u241_usb_audio_io` — daemon golden ABI7 (Local/Windows).
- `lgpt/otg/bin/r36s_sp404_host_audio_io` — daemon SP404MKII (Sampler, UAC2
  host, passthrough puro 48 kHz + FIFO_DUMP diagnóstico).
- `lgpt/otg/bin/r36s_midi_host_io` — daemon USB-MIDI host.
- `lgpt/otg/bin/otg_h37_apply_driver_mode.sh` — selector de driver ABI7.
- `lgpt/otg/bin/otg_h37_android_runtime_supervisor.sh` — supervisor Android h36.
- `lgpt/otg/bin/otg_h37_host_runtime_supervisor.sh` — supervisor del host.
- `lgpt/otg/bin/otg_h37_host_device_detect.sh` — detección de dispositivos host.
- `lgpt/otg/bin/r36s_aoa_bulk_audio_io_h36` + `r36s_aoa_bulk_receiver_h36` — daemons Android h36.
- `ANDROID/LGPTUsbAudioBridge-H36-debug.apk` — APK del puente de audio Android.

## Checksums

```text
core   5685150957fbdfcaca9d38afcf5d4753114c48e2b0b40abe34cfee2130f7d1cb
daemon 53258f2b8b3749c866af248814eb147f0762a1b17cfffb644adb573167b52815
sp404  924c48436086c2b84897a2bde0547e5e8b9381a60a4845ebcd82fc8aab47961a
midi   3f0ea7a23db7390f1fb3b73cbda97f66316c6568d0c7574b838579a014baee80
```

## Instalación

1. Partir de una SD con sistema Stock y TreeFrogUI.
2. Extraer el ZIP de la release.
3. Copiar las carpetas `cubegm`, `lgpt`, `roms`, `LGPT_OTG_LOGS` y `ANDROID`
   del ZIP directamente a la raíz de la SD, combinando carpetas y reemplazando
   archivos.
4. Instalar `ANDROID/LGPTUsbAudioBridge-H36-debug.apk` en el dispositivo
   Android (opcional, solo para el modo Android).
5. Expulsar la SD de forma segura.
6. Iniciar LGPT desde TreeFrogUI.

## Modos de audio

1. **Local** — salida local de la consola.
2. **Windows** — LGPT como adaptador USB-C audio-out para PC.
3. **Android** — puente de audio USB con el dispositivo Android vía APK.
4. **Sampler** — SP404MKII como sampler (EXT SOURCE), entrada USB-UAC2.

## Fuentes y reproducibilidad

El ZIP de la release contiene:

- `cubegm/`, `lgpt/`, `roms/`, `LGPT_OTG_LOGS/` y `ANDROID/`: payload listo
  para copiar directamente a la raíz de la SD.
- `SOURCE_AND_TOOLS/full_repository/`: snapshot completo del repositorio en el
  commit publicado.
- Scripts de auditoría, compilación, verificación, instalación, rollback y
  recolección de logs.
