# LGPT R36SX H38.2 — Tres drivers de audio (Local / Windows / Android)

Versión H38.2, ABI7, three-mode, frontend-safe.

## Alcance

Esta versión consolida el port con tres drivers de audio USB seleccionables,
la vista Mixer con FX por instrumento, la sidebar (tabla de patrones abajo a
la derecha) y phrase FX reducido a beatmaking.

## Contenido

- `cubegm/cores/lgpt_r36sx_port_libretro.so` — core H38.2 (ABI7, 3 modos).
- `lgpt/otg/bin/r36s_u241_usb_audio_io` — daemon golden ABI7 (Local/Windows).
- `lgpt/otg/bin/otg_u241_apply_profile_once.sh` — aplicador de perfil 3 modos.
- `lgpt/otg/bin/otg_h37_apply_driver_mode.sh` — selector de driver ABI7.
- `lgpt/otg/bin/otg_h37_android_runtime_supervisor.sh` — supervisor Android h36.
- `lgpt/otg/bin/r36s_aoa_bulk_audio_io_h36` + `r36s_aoa_bulk_receiver_h36` — daemons Android h36.
- `ANDROID/LGPTUsbAudioBridge-H36-debug.apk` — APK del puente de audio Android.

## Checksums

```text
core   9f37e01725a084e291d1d50bb9dfb9493f5fc4473caa86d3c7f1cbe3cd408d8e
daemon 53258f2b8b3749c866af248814eb147f0762a1b17cfffb644adb573167b52815
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

## Fuentes y reproducibilidad

El ZIP de la release contiene:

- `cubegm/`, `lgpt/`, `roms/`, `LGPT_OTG_LOGS/` y `ANDROID/`: payload listo
  para copiar directamente a la raíz de la SD.
- `SOURCE_AND_TOOLS/full_repository/`: snapshot completo del repositorio en el
  commit publicado.
- Scripts de auditoría, compilación, verificación, instalación, rollback y
  recolección de logs.
