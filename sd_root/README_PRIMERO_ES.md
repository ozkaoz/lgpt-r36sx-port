# LGPT R36SX Bacon 1.1 — Audio Driver Refact, Sampler Audio Added (iteración local U2.52.4)

Versión Bacon 1.1 (baseline), ABI7, cuatro drivers de audio (Local / Windows /
Android / Sampler), frontend-safe, con las modificaciones locales actuales.

## Alcance

Este payload mantiene la base de audio estable de Bacon 1.1: el refactor del
driver de audio Sampler (SP404MKII), el passthrough puro 48 kHz sin pitido
permanente, y los cuatro drivers de audio USB seleccionables funcionando.
Sobre esa base añade las modificaciones locales:

- **SIGPIPE harden**: el core ignora SIGPIPE en `retro_init()`, eliminando el
  crash (exit 141) del modo Sampler/Windows cuando el lector del fifo
  desaparece durante el switch de modo.
- **EQ paramétrico y gráfico**: EQ de 8 bandas por instrumento de sample
  (bell / low shelf / high shelf / low pass / high pass / notch, respuesta
  RBJ) con máscara de bandas por pad y bypass global, más analizador de
  espectro en vivo (FFT) sobre el overlay del modal.
- **Nombre aleatorio de proyecto**: la tecla SELECT en el diálogo New genera
  un nombre aleatorio (formato adjetivo-verbo, estilo djdiskmachine LGPT) con
  retry anti-colisión contra proyectos existentes.

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
core   e94165012a86d79f3bccd8725aebec40c2350248f28fb64ae526feca989a45d9
daemon 9537478789115e6a83ef12820f4cf8f8d307b3dc55fb847fd2b13b988268b831
sp404  fc90e7312b272de6168bd9c7ade3902eebf11ec56f54e43e71416265695395cf
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
