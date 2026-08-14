# F9 - Riesgos y limites del camino critico de audio

Tramo del refactor `refactor/bacon-1.2.1-preserve` (golden Bacon 1.2.1).
Sin cambio de binario: core `ea7a80e4` y daemon `4be71632` intactos.
F9 documenta, con referencias exactas al codigo, la cadena de audio
critica y sus limites actuales, y registra como deuda las oportunidades
de optimizacion que NO se implementan (ninguna tocar timings ni
comportamiento observable).

## 1. Cadena critica: retro_run -> FIFO -> ASRC -> daemon

```
retro_run (core, TreeFrogLibretro.cpp)
  -> mezcla final del core (MixerService)
  -> resample a 48 kHz stereo S16_LE
  -> TreeFrogUac2Bridge (core side, H39/H40)
  -> FIFO en tmpfs  (/tmp/r36sx_*_pcm_fifo, O_NONBLOCK)
  -> daemon host (device/r36s_*.c)
       drain_fifo -> ring buffer -> ASRC (correccion de reloj)
       -> ALSA/USB -> DAC o dispositivo USB
  monitor: CAPTURE_MONITOR_FIFO U2517 -> AudioEngineMonitorStep (F4)
```

Puntos de acople (con refs):

| Eslabon | Codigo | Nota |
|---|---|---|
| Budget retro | `TreeFrogLibretro.cpp` H39 | Nunca descarta budget de wall-clock; cap 1 s (48000 frames), chunks <=2048. Un stall del frontend >43 ms se compensa entero, no se pierde ~2.3% |
| Staging FIFO | `TreeFrogUac2Bridge.cpp` H40 | Pending acotado a 16384 samples; writes parciales O_NONBLOCK, sin bloqueo al core |
| FIFOs | `kFifo=/tmp/r36sx_uac2_bridge_fifo`, `kAoaPcmFifo=/tmp/r36sx_aoa_bulk_pcm_fifo`, `kCaptureMonitorFifo=/tmp/r36sx_usb_capture_monitor_fifo` (bridge); daemons: `/tmp/r36sx_sp404_pcm_fifo`, `/tmp/r36sx_midi_pcm_fifo` | Todo en tmpfs; un FIFO congelado degrada a silencio, nunca a audio stale |
| ASRC | u2523:572-573, sp404:949-950 | Ver limites en tabla siguiente |
| Recoveries | u2523 prepare fallback 480 periods; sp404 reenum escalado | Ver riesgos |

## 2. Limites actuales (valores y refs)

| Limite | Valor | Refs |
|---|---|---|
| Correccion ASRC maxima | `ASRC_MAX_CORRECTION_PPM` 1200 ppm (ambos daemons) | u2523:572, sp404:949 |
| Integral ASRC | 1000 ppm (u2523) vs 30000 ppm (sp404) | u2523:573, sp404:950 |
| Backlog objetivo/prime | `ASRC_TARGET_BACKLOG_FRAMES` / `ASRC_PRIME_BACKLOG_FRAMES` 2400 frames (~50 ms @48 kHz) | u2523:570-571 |
| Hold floor anti-stall | `ASRC_HOLD_FLOOR_FRAMES` 2400 frames (~50 ms) | sp404:957-958 |
| Re-enumeracion USB (SP404) | max 8 intentos, ventana >=30 s entre intentos, arranca tras 5 s de stall; alterna authorized-toggle y USBDEVFS_RESET; al agotar el daemon escribe `sp404-stall-exhausted` y sale (codigo 3) para backoff del supervisor | sp404:2925-2975 |
| Budget retro cap | 1 s (48000 frames) | TreeFrogLibretro.cpp (H39) |
| Staging FIFO core | 16384 samples | TreeFrogUac2Bridge.cpp (H40) |
| Prebuffer monitor | 960 frames; gain 75% | AudioEngine.h (F4) |
| USB-REC staging | `/tmp/r36sx_lgpt_record/` (tmpfs); sin fallback a SD | u2523:178 |

Divergencia observada: la integral ASRC del sp404 es 30x la del u2523.
Del lado u2523 (UAC2, host siempre presente) el reloj se corrige con
suavidad; del lado sp404 la correccion tolera deficits largos del host
SP-404. Es comportamiento deliberado del golden; se documenta como deuda
(no se unifica sin medir) en la seccion 4.

## 3. Riesgos por eslabon

- **Frontend stall (retro gap)**: H39 lo absorbe hasta 1 s; por encima el
  cap descarta budget. Riesgo: latencia/aliasing residual si el frontend
  se queda enganchado >1 s de forma sostenida.
- **FIFO congelado**: degrada a silencio (comportamiento intencional);
  riesgo: el usuario puede no distinguir silencio de congelacion de la
  vista; mitigado por BRIDGE_PROGRESS (source_silence / starvation_events).
- **ASRC contra drift**: si el reloj del dispositivo derivara >1200 ppm
  de forma sostenida, el ASRC satura al tope y la latencia crece/decae
  lentamente hasta que la re-open resetea el ring (u2523 U2.63.1).
- **Backlog 2400**: ~50 ms de latencia maxima introducida por el anillo;
  el prime se repone en cada fresh open (drop del FIFO stale).
- **Reenum SP404 agotado**: tras ~4 min de fallos el daemon se rinde a
  proposito (H43: libera el rol host para que el cambio de modo funcione);
  riesgo: silencio hasta que el supervisor reintente (backoff).
- **Capture EIO storm**: mitigado con la misma ventana de reenum y
  `stop_capture("daemon-stop")` limpio al salir.
- **Latency trim**: eventos contados (latency_trim_events) sin alarma; si
  creciera de forma sostenida indicaria drift real del reloj.

## 4. Deuda tecnica y oportunidades (NO implementadas)

Solo se documentan; ninguna se toca porque alteraria timings o
comportamiento observable, prohibido por el roadmap:

1. **Unificar la politica de integral ASRC** (1000 vs 30000): requeriria
   medir en hardware; hoy es divergencia deliberada por modo de reloj.
2. **Extraer las constantes ASRC a un header compartido** entre los
   daemons (hoy duplicadas en u2523 y sp404): refactor puro de
   constantes, seguro, pero fuera de alcance si no cambia binario.
3. **Exponer el PPM de correccion actual en BRIDGE_PROGRESS**: solo
   observabilidad; no cambia comportamiento.
4. **Alarma de latency_trim_events sostenido**: monitoring, no logica.
5. **Unificar los paths runtime** en un solo dir tmpfs ya acotado
   (`/tmp/r36sx_lgpt_usb/`): hoy ya todos los daemons escriben en /tmp
   (politica F5 cumplida); queda consolidar constantes duplicadas.

Ninguna escritura PERIODICA runtime bajo `/mnt/sdcard`: todo el estado
del camino critico vive en /tmp (tmpfs), coherente con StoragePolicy (F5).
Las unicas excepciones son config persistente escrita POR EVENTO
(sentinel `lowlat_240`, `audio_driver_mode`, ambos
`/mnt/sdcard/lgpt/otg/`) y el mirror diagnostic en `/otg/logs` (categoria
Diagnostic de F5); los bucles de audio nunca escriben en SD.

## Evidencia

- Audit `AUDIT_CLEAN_MAIN_U2523_OK`.
- Core `ea7a80e4` + daemon `4be71632` byte-identicos (F9 solo documenta).
- Gate diag `NO_DIAGNOSTICS_OUTSIDE_DEVICE` (mismos 4 warnings
  preexistentes).
- Baseline `F9_BASELINE_OK` (coherencia docs <-> codigo de los limites).
