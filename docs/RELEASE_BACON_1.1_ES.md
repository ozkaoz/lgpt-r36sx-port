# LGPT R36SX — Bacon 1.1 — Audio Driver Refact, Sampler Audio Added

Release estable de los cuatro drivers de audio del port LGPT R36SX.

## Qué hay de nuevo en Bacon 1.1

### Sampler (SP404MKII) — pitido permanente eliminado (U2.53.0)

- Rearmer desde cero del módulo playback del daemon `r36s_sp404_host_audio_io`:
  se elimina el resampler polifase, el feed EMA, el feed-ratio y los
  latency-trims.
- Playback ahora es passthrough puro ring→PCM: pop 2 canales, gain 0.65,
  conversión a 4 canales (L/R en ch1/2), silencio de período completo en
  starvation (nunca se re-emite audio stale) y drop-on-full del ring.
- El diagnóstico `FIFO_DUMP` confirmó que el contenido del fifo del core es
  audio limpio del proyecto: el tono full-scale permanente provenía del path
  del daemon (re-emisión de buffers stale tras un cambio de modo sin replug).

### Fix de crash del daemon (U2.53.1)

- Overflow del buffer `FIFO_DUMP`: el array de 32768 muestras (16384 frames
  estéreo) se indexaba como frames ×2 (índice hasta 65535); desde el frame
  16384 (~341 ms de audio) el daemon escribía fuera de límites y moría con el
  primer golpe del proyecto, dejando el fifo sin lector (port congelado).
- Fix: `FIFO_DUMP_BUF_FRAMES 32768` con buffer `[32768*2]` y guard/flush
  coherentes.

### RC9.8 — fix del pitido constante del Sampler

- Feed EMA reseteado en cada open + rechazo de spikes transitorios + override
  de starvation (ratio 1.0).

### RC9.7 — etiqueta y barra USB-REC

- Etiqueta `Sampler` sin sufijo `[OUT]`, barra de preescucha USB-REC sólida
  (`UIIntVarField`), descripción `SP404: console sound to sampler (EXT SOURCE)`.

## Verificación

- Build: `BUILD_U2523_OK` (core sha `5685150957...`, daemon SP404
  `924c484360...`).
- Install + verify en SD: `VERIFY_U2523_OK`, `ERRORS=0`.
- Retest en consola: los cuatro drivers (Windows / Local / Android / Sampler)
  funcionando correctamente.

## Contenido del ZIP

- `cubegm/`, `lgpt/`, `roms/`, `LGPT_OTG_LOGS/` y `ANDROID/`: payload listo
  para copiar a la raíz de la SD (copia de raíz completa, incluye la APK del
  puente de audio Android `LGPTUsbAudioBridge-H36-debug.apk`).
- `SOURCE_AND_TOOLS/full_repository/`: snapshot completo del repositorio
  (código fuente, changelog, tests, scripts de build/install/verify).
- `SOURCE_AND_TOOLS/RELEASE_MODE.txt`: modo `autonomous`.

## Instalación

1. Partir de una SD con SO Stock + TreeFrogUI.
2. Extraer el ZIP.
3. Copiar `cubegm`, `lgpt`, `roms`, `LGPT_OTG_LOGS` y `ANDROID` a la raíz de
   la SD (combinar y reemplazar).
4. Instalar la APK en el dispositivo Android (opcional, solo modo Android).
5. Expulsar, iniciar LGPT desde TreeFrogUI.
