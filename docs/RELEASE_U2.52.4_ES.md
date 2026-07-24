# LGPT R36SX U2.52.4 — Copy-root + ALSA/UAC2 validado

Fecha de validación: 24 de julio de 2026.

## Alcance

Esta versión consolida dos correcciones probadas en una R36SX con sistema Stock y TreeFrogUI:

1. Instalación copy-root: el usuario extrae el ZIP y copia sus carpetas directamente a la raíz de la SD. No es necesario ejecutar un instalador `.sh` desde Windows.
2. Audio USB UAC2: se incorporan los módulos ALSA requeridos por `usb_f_uac2.ko` para el kernel Stock `4.4.186-release`.

## Corrección del Sampler

El launcher crea y verifica antes de iniciar LGPT:

- `/mnt/sdcard/lgpt/samples`
- `/mnt/sdcard/lgpt/samples/records`
- `/mnt/sdcard/lgpt/samplelib`
- `/mnt/sdcard/lgpt/instruments`
- `/mnt/sdcard/lgpt/project`
- `/mnt/sdcard/lgpt/projects`
- `/mnt/sdcard/lgpt/tmp/record`
- `/mnt/sdcard/lgpt/usbrecs`

También recupera `config.xml` desde `config.stock.xml` únicamente cuando falta o está vacío. No sobrescribe configuraciones válidas.

## Stack ALSA/UAC2

Orden de carga validado:

```text
soundcore.ko
snd.ko
snd-timer.ko
snd-pcm.ko
usb_f_uac2.ko
```

Todos los módulos usan:

```text
4.4.186-release preempt MIPS32_R2 32BIT
```

Checksums SHA-256:

```text
soundcore.ko  5cd5d4dbdb0ce7379c64611d035bf3643d9f6d3097c046bb49214b2f065d5f39
snd.ko        91742747579d9a6e8ca0fff0e920eed69afdd9f2fcab57029e351ea13c5f95bb
snd-timer.ko  25cb2142f7bea8f92edfc03d28c3a1e82cc656a1aa09ec5a5e1a99be41c87920
snd-pcm.ko    48f7d39e97aafb6f61c2ff2f8c9a2a101115875924e22d986f8fc8485f4a2704
usb_f_uac2.ko e9062ac5a37aa7706c98a3411a57dbe67bc98ef82621c430c1d4e7a89b2fbefe
```

## Validación real

Se verificó en hardware:

- Inicio normal de LGPT.
- Acceso a Instrument → Sampler.
- Carga y reproducción de muestras.
- Secuenciación estable.
- Audio interno operativo.
- Enumeración y funcionamiento de audio USB.
- Recolección de logs y desmontaje controlado de la SD.

## Instalación

1. Partir de una SD con sistema Stock y TreeFrogUI.
2. Extraer el ZIP de la release.
3. Copiar las carpetas `cubegm`, `lgpt`, `roms` y `LGPT_OTG_LOGS` del ZIP directamente a la raíz de la SD, combinando carpetas y reemplazando archivos.
4. Expulsar la SD de forma segura.
5. Iniciar LGPT desde TreeFrogUI.

## Fuentes y reproducibilidad

El ZIP de la release contiene:

- `cubegm/`, `lgpt/`, `roms/` y `LGPT_OTG_LOGS/`: payload listo para copiar directamente a la raíz de la SD.
- `SOURCE_AND_TOOLS/full_repository/`: snapshot completo del repositorio en el commit publicado.
- Scripts de auditoría, compilación, finalización, despliegue, rollback y recolección.
- Configuración del build, símbolos y evidencia de validación.
