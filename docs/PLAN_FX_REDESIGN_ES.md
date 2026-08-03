# PLAN_FX_REDESIGN_ES — Rediseño del sistema de efectos LGPT R36SX

Base: release "LGPT R36SX - Bacon 1.0" (tag `Bacon-1.0`, commit `2e43e81`).
Hardware objetivo: R36SX v2.6. Frecuencias: 44100 Hz local (libretro), 48000 Hz USB/ALSA.
No es una copia del tracker M8; es una arquitectura de efectos propia inspirada en conceptos modernos.

---

## A. Mapa de flujo de audio (actual → objetivo)

### Actual (ruta render)
```
libretro frame (60 fps, 735 muestras @44.1k)
  → TreeFrogLibretro.cpp:1214-1226  (Render(audio_buffer, frames))
    → TreeFrogAudioDriver::Render  (TreeFrogAudioDriver.cpp:234-275)
      → AudioDriver.cpp:77-81  (notifica ADET_BUFFERNEEDED)
        → Player::Update → moveToNextStep (Player.cpp:577)
          → playCursorPosition (Player.cpp:889-1015)
            → PlayerMixer::StartInstrument (PlayerMixer.cpp:120-125)
              → PlayerChannel::StartInstrument (PlayerChannel.cpp:24-33)
                → SampleInstrument::Start (SampleInstrument.cpp:170-398)
          → SampleInstrument::Render (SampleInstrument.cpp:475+)
              → renderParams_[channel] actualizados por SRPUpdaters.cpp
              → filtros en línea (Filters.cpp), feedback comb (líneas 635-910),
                bitcrusher (555-569), interpolación, loop
              → mezcla de canales + efectos en AudioMixer.cpp:71-181
```

### Defectos confirmados (auditoría con file:line)

| # | Defecto | Evidencia | Impacto |
|---|---------|-----------|---------|
| 1 | DLAY es retardo de disparo (evento), no audio delay | Player.cpp:875-885, 583-589 | No hay delay como efecto |
| 2 | FBMX/FBTN = feedback de una sola línea (comb) | SampleInstrument.cpp:635-910 | No feedback multinivel |
| 3 | Buffer feedback se borra al iniciar voz | SampleInstrument.cpp:348 | Pierde cola de voz anterior |
| 4 | Cola deja de procesarse al terminar la voz | Render corta cuando no hay voz | Tail cortado |
| 5 | Mapeo FBTN discontinuo | SampleInstrument.cpp:446-450 (`*10`) | Curva no monótona, saltos |
| 6 | Feedback gain puede ≥1 (inestable) | SampleInstrument.cpp:644 (`*4.0`) | Zumbido/clipping |
| 7 | CRSH mal descrito (es bitcrusher + downsample) | SampleInstrument.cpp:553-569 | Confuso |
| 8 | CRSH shift negativo con signo (UB) | SampleInstrument.cpp:555-559 | UB en hardware |
| 9 | Bypass de profundidad 0 no funciona (mask 0xFFFFFFFF) | SampleInstrument.cpp:556-559 | No se puede desactivar |
| 10 | Drive actúa como atenuación (crushvol=drive/255) | SampleInstrument.cpp:563-564 | Semántica invertida |
| 11 | Filtro dependiente de 44.1kHz | Filters.cpp:54 (`1/22050.0f`) | Distorsión @48k USB |
| 12 | No hay EQ | — | Ausente |
| 13 | No hay compresor/limitador | — | Ausente |
| 14 | No hay buses delay/reverb | — | Ausente |
| 15 | No hay tests DSP | tests/ sin cobertura DSP | Riesgo |

### Objetivo (FxEngine)
```
MIXER INPUT (dry por canal ya procesado)           PlayerMixer / canales
  ├─ Send delay (por pista) ──→ Return Delay Estéreo ──→ (opcional → Send Reverb)
  ├─ Send reverb (por pista) ──→ Return Reverb Estéreo
  └─ Dry directo
        ↓
   EQ Returns (bandas) ──→ mezcla buses + dry
        ↓
   EQ Master (3 bandas) ──→ Compresor/Limitador Master
        ↓
   Soft Clip final (existente, AudioMixer softClipData_) ──→ salida
```

Principio: el FxEngine es un módulo nuevo desacoplado (`Application/Audio/FxEngine/`) que consume los canales ya renderizados y emite el bus final. La ruta legacy se conserva como modo de compatibilidad.

---

## B. Inventario de estado DSP

### Canales / voces
| Estado | Tipo | Tamaño | Ubicación | Persistente tras nota? |
|--------|------|--------|-----------|------------------------|
| `renderParams_[SONG_CHANNEL_COUNT]` | SampleRenderingParams | 8 canales | SampleInstrument | Vuelve a defaults en Start |
| `feedback_[8][3500*2]` | fixed (Q15) | 8 × 7000 × 4B = 224 KB | static SampleInstrument.cpp:22 | NO — memset en Start (348) |
| `filter[channel]` | filter_t | 8 voces | Filters.cpp | Recalculado por parámetro |
| `baseSpeed_/speed_` (pitch) | | por instrumento | SampleInstrument | por instrumento |
| Variables de loop/interp | | por voz | Render | en Render |

### Mezclador
| Estado | Tipo | Ubicación |
|--------|------|-----------|
| `masterVolume_` | int | AudioMixer.h:44 |
| `softClipData_[4]` | fixed | AudioMixer.h:39 |
| `clipped_`, `peakValue_` | bool/fixed | AudioMixer.h:49-50 |
| `dampCached_`, `softclipGain_` | | AudioMixer.h |

### Persistencia (formato proyecto)
- Archivo `lgptsav.dat`, XML TinyXML, raíz `LITTLEGPTRACKER`, versión = atributo `VERSION` del nodo `PROJECT`.
- Sin magic ni hash (PersistencyService.cpp:14-32).
- Nodo propio del port: `PITCHES` (fallback pitch 0). Rotura deliberada V1→V2 documentada en RELEASE_H38.7_ES.md.

---

## C. Presupuesto de memoria

### Actual (estático, sin contar sample buffers)
| Bloque | Bytes | Notas |
|--------|-------|-------|
| feedback_ | 224 KB | 8×3500×2×4B |
| buffer de mezcla temporal | malloc por llamada (AudioMixer.cpp:82-84) | ELIMINAR (prohibido en RT) |
| Filters banco | ~8 × filter_t | pequeño |
| Fuentes de samples | dependen de .lsdsx | no tocar |

### Objetivo (nuevo, todo estático y preasignado)
| Bloque | Estimación | Notas |
|--------|-----------|-------|
| Delay estéreo @44.1k | 2 × 44100 × 4B × ~2s = ~706 KB | preasignado, circular |
| Delay estéreo @48k | reservar 2 × 48000 × 4B = ~768 KB | máximo fijo |
| Reverb Dattorro | ~100 KB (8 delays + FDN optional) | frente a FDN 4/8: +200 KB |
| FxEngine buses (send/return/dry) | 8 × 735 × 4B × 4 ≈ 94 KB | buffers por frame |
| Feedback legacy | 224 KB | conservado en modo legacy |
| Coeficientes EQ/comp | <1 KB | precalculados control-rate |
| Total incremental | ~1.4-1.6 MB | dentro de presupuesto R36SX (128 MB) |

Regla RT: 0 `malloc`/`new` en callback; todos los buffers son static o preasignados al arranque.

---

## D. Presupuesto de CPU

### Medida actual (estimar al inicio de Fase 1 en hardware real)
- Render 60 fps × 735 muestras. Cadena actual ya es O(n·canales·samples).
- Filtros biquad: ~5 mults/sample/banda (fp_mul). Feedback comb ya es O(n).
- Uso de `fp_*` (Q15 en fixed.h, FIXED_SHIFT=15) en toda la cadena — NO se asume FPU.

### Objetivo por bloque (cota superior estimada)
| Bloque | Coste/sample/canal (Q15) | Coste total 8 canales |
|--------|--------------------------|------------------------|
| Biquads EQ (3 bandas) | ~5 ops | ~120 ops/sample |
| Delay lectura fraccional (linear) | ~6 ops | — |
| Reverb Dattorro (2 buses) | ~40 ops | ~80 ops/sample |
| Compresor/limitador | ~15 ops | estéreo |
| **Incremento estimado** | | **< 0.5× del coste actual de render** (medir antes de declarar) |

Decisiones:
- Todos los coeficientes (filtros, EQ, comp) se recalcular solo cuando cambia un parámetro (control rate), nunca por muestra.
- Evitar `%` en loops de buffer (usar máscara para potencias de 2 o contadores).
- No asumir FPU/SIMD; comparar float vs Q1.15 vs Q1.31 con benchmarks en la R36SX v2.6 real.
- Solo se usa interpolación lineal por defecto (cúbica solo tras benchmark).
- "No declares que está optimizado hasta medirlo en la R36SX v2.6 real."

---

## E. Riesgos de compatibilidad y mitigaciones

| Riesgo | Probabilidad | Mitigación |
|--------|--------------|------------|
| Proyectos legacy suenan distinto | Alta | FxEngine en bypass por defecto; modo legacy activable por proyecto; golden tests |
| Cambios de formato (nuevos parámetros FX) | Media | Solo añadir campos con defaults = legacy; versión persistencia V2→V3 con fallback |
| CPU insuficiente en R36SX | Media | Presets ECO por defecto; medir antes de afirmar; reverb opcional |
| Latencia/memoria USB 48k | Media | Coeficientes dependientes del sample rate real (fijar 22050 hardcoded) |
| Regresión de la columna Pitch (trabajo previo) | Baja | Golden tests del nodo PITCHES + chops |
| NaN/Inf/overflow Q15 en realimentación | Alta (actual) | Guardas por muestra, límite de loop gain <1, sanitizers, test de stress |
| Mapeos de parámetros rotos (hex→decimal) | Media | Tablas de mapeo monotónicas 00-FF, pruebas de curva, docs |

---

## F. Plan por fases

Cada fase: archivos modificados, justificación, diagramas, coste memoria/CPU, resultados tests, riesgos, rollback.

### Fase 0 — Fundaciones (sin tocar el render principal)
- **Tests DSP**: añadir `tests/` para: impulso, silencio, DC step, seno, ruido, barridos, cambios abruptos, NaN/Inf.
- **Corregir UB de CRSH**: `SampleInstrument.cpp:555-559` — eliminar shift negativo; profundidad 4-16 bits con bypass real (mask=0 cuando profundidad=16).
- **Renombrar/mapear comandos**: `DLAY`→Note Delay, `FBMX/FBTN`→Legacy Comb Feedback, `CRSH`→Bit Crusher (solo HelpLegend/CommandList, sin cambiar ID de command para no romper .lsdsx).
- **Benchmarks**: script que mide render actual por bloque.
- **Golden legacy WAVs**: capturar salida actual para comparación.
- **Hex→decimal global**: sustituir valores hex del port por decimal o etiquetas descriptivas.
- Entregable: `docs/PLAN_FX_REDESIGN_ES.md` actualizado + tests verdes + golden.

### Fase 0 — RESULTADO (aplicado y verificado)
Cambios aplicados sobre la base `2e43e81` y verificados con el toolchain MIPS real
(`mips-mti-linux-gnu-g++` de buildroot; `-fsyntax-only` limpio, solo warnings
preexistentes `-Wwrite-strings`). El requisito hex→decimal queda **pendiente
(decidido por el usuario: "No tocar hex todavía")** y se hará fase por fase.

1. **CRSH UB corregido** — `source/sources/Application/Instruments/SampleInstrument.cpp`:
   - Bloque crush (antes líneas 555-559): ahora `crushBits` se clamp a [0,16],
     `mask` es `unsigned int` (`0xFFFFFFFFu`), shift nunca negativo ni >31.
     Semántica documentada en el código: 16 = bypass (máscara completa), menor = más cuantización.
   - `downsample_` también clamp a [0,31] para eliminar el UB del `0xFFFFFFFF<<downsmpl`.
   - Aplicación de máscara: `s2=(fixed)((unsigned int)s2 & mask)` (sin signo/unsigned UB).
2. **Renombres (solo etiquetas, los FourCC no cambian — compatibilidad .lsdsx)**:
   - `Utils/HelpLegend.h`: display names `RVB/RVT/CMP` → `FBM/FBT/BTS`; textos de ayuda
     actualizados (`bit CrusH:aa-b drive aa (max 128) bit depth -b (max 8)`,
     `legacy FBack mix/tune`, `DeLAY note`).
   - `Instruments/CommandList.cpp`: comentario de cabecera actualizado
     (legacy comb feedback / note delay / bit crusher).
3. **Tests DSP nuevos** (integrados en `scripts/audit.sh` vía glob `test_*.py`):
   - `tests/test_fx_phase0_bitcrusher_model.py` → `DSP_BITCRUSHER_PHASE0_OK`
     (sin UB en todo [0,255], bypass exacto, monotonicidad, markers en el fuente).
   - `tests/test_fx_phase0_inline_lowpass_model.py` → `DSP_INLINE_LOWPASS_PHASE0_OK`
     (impulso finito, paso de DC con cutoff completo, sin NaN/Inf, constante 22050 documentada).
   - `tests/test_fx_phase0_golden_legacy.py` → `GOLDEN_LEGACY_PHASE0_OK`
     (13 WAVs presentes, 16-bit mono 44.1k, crush16 = bypass bit-exacto, crush0 = onda 1-bit).
4. **Benchmark** — `scripts/bench_dsp.sh` (host, baseline Q15):
   - fp_mul/op 0.216 ns · fp_add+sub pair 0.434 ns · fp_div/op 1.405 ns · i2fp+fp2i pair 0.432 ns.
   - Números reales de la R36SX v2.6 pendientes (sección D del plan).
5. **Golden legacy** — `scripts/generate_golden_legacy.sh` → `validation/PHASE0_GOLDEN/`
   (13 WAVs deterministas: impulse/sine 440/sine 8k/noise/DC raw + crush16/8/4/0 + lowpass).

Verificación: los 4 fallos existentes de `test_u2520/u2521/u2522/u2523` son **preexistentes**
(confirmado con `git stash -u` sobre `2e43e81`); no están relacionados con Fase 0.

### Fase 1 — Esqueleto FxEngine en bypass
- Nueva carpeta `Application/Audio/FxEngine/` con buses dry/send/return (fijos, sin RAM dinámica).
- Se conecta a la salida del mezclador pero con ganancia de bypass = 1,0 y sin DSP.
- Medir: CPU/callback/memoria con y sin FxEngine activo.
- Criterio: 0 allocs/syscalls en callback (verificable con contador).

### Fase 1 — RESULTADO (aplicado y verificado)
1. **Nuevos archivos** — `source/sources/Application/Audio/FxEngine/FxEngine.h/.cpp`:
   - `FxEngine::Buses`: buses estáticos `dry_/send_` (8 canales) y `returnDelay_/returnReverb_/master_`
     (estéreo), Q15, **311,296 bytes** (~304 KB) de RAM estática, cero malloc.
   - `FxEngine::Process(fixed*, samplecount)`: etapa master post-mix. `legacyMode_ == true`
     por defecto → bypass puro (gain 1.0, el buffer no se toca) → salida bit-idéntica a legacy.
   - Contadores RT: `callCount_`, `frames_`, `maxFrames_`, `rtViolations_` (debe quedar en 0).
   - **Sin `System::GetClock()` en el callback**: es wall-clock (syscall), prohibido en RT; la
     medición de CPU se hace en host (ver bench abajo) y los contadores en device.
2. **Integración** — `AudioOutDriver::Trigger()` (AudioOutDriver.cpp:49-53) y
   `DummyAudioOut::Trigger()` (DummyAudioOut.cpp:31-36) llaman
   `FxEngine::FxEngine::GetInstance().Process(...)` **después** de `AudioMixer::Render` y
   antes de `clipToMix()` / render a archivo. Así cubre playback y render a WAV.
   `source/projects/Makefile`: dir `Application/Audio/FxEngine` añadido a `COMMONDIRS` y
   `FxEngine.o` a `COMMONFILES`.
3. **Test** — `tests/test_fx_phase1_fxengine_bypass.py` → `FXENGINE_BYPASS_PHASE1_OK`:
   bypass 1:1 (buffer intacto), invalid inputs flag `rtViolations_`, scan del módulo sin
   malloc/new/delete/free/fopen/printf/open/write/syscall/GetClock/Trace/log, buses
   estáticos con footprint < 2 MB, puntos de integración tras `AudioMixer::Render` en ambos
   Triggers, Makefile registra el módulo.
4. **Compilación MIPS real** — `mips-mti-linux-gnu-g++` (toolchain SF3000/R36SX v2.6)
   syntax-check limpio de `FxEngine.cpp`, `AudioOutDriver.cpp`, `DummyAudioOut.cpp`.
5. **Benchmark host** — `scripts/bench_fxengine.sh` → `BENCH_FXENGINE_PHASE1_OK`:
   - `Process` bypass: **2.374 ns/callback** (host -O2; vs fp_mul/op 0.216 ns del baseline).
   - `StaticMemoryBytes` = 311,296 B · 201,000 callbacks → `RtViolations = 0`.
   - Números reales de la R36SX v2.6 pendientes (sección D del plan).

Verificación: en modo legacy el bypass es 1:1, por lo que los golden WAVs de Fase 0 y la
reproducción actual no cambian (confirmado por el test de bypass y por `GOLDEN_LEGACY_PHASE0_OK`).

### Fase 2 — Delay y Reverb
- Delay estéreo (retardo real): buffers circulares preasignados, ms libres, sync musical (divisiones), L/R, feedback estable (loop gain máx documentado), ping-pong, width, HP/LP en loop, saturación opcional, interpolación lineal (cúbica opcional), smoothing, crossfade, tail independiente, bypass sin discontinuidad.
- Reverb: Dattorro/Griesinger simplificada vs FDN 4/8 líneas (comparar densidad/costo). Predelay, difusión, size, decay/RT60, damping, HP/LP entrada, mod, width, return, modos ECO/NORMAL. Cola independiente. Protección runaway/NaN/DC. Sin convolución.

### Fase 2 — RESULTADO (aplicado y verificado)
1. **DelayLine** — `source/sources/Application/Audio/FxEngine/DelayLine.h/.cpp`:
   - Buffer circular interleaved **preasignado**: `kBufferSize = 96.000 muestras/canal × 2`
     (= 2000 ms @ 48 kHz) → 768 KB estáticos, cero malloc.
   - Tiempo libre en ms con interpolación lineal; cambios con **glide** (0,5 muestras/llamada)
     para evitar clics; feedback clamp a **[0, 0.98]** (loop gain < 1, sin runaway);
     **ping-pong** (cruce L/R en el lazo), **width mid/side**, **HP/LP one-pole opcionales**
     en el lazo de feedback, saturación opcional, crossfade de mix (bypass → dry sin cortar
     la cola: **tail independiente**), `SyncDivisionToMs(division, bpm)` para sync musical.
2. **Reverb** — `source/sources/Application/Audio/FxEngine/Reverb.h/.cpp`:
   - Schroeder/Dattorro simplificada: input HP/LP one-poles → **predelay** (máx 100 ms
     @ 48 kHz) → **4+4 combs** (NORMAL) / **2+2** (ECO) con damping LP en el lazo →
     **2+2 allpass** (1+1 ECO) → width mid/side → **DC blocker**.
   - RT60 controlado por `g = 10^(-3·L/(RT60·fs))` (de donde RT60 se mide en el modelo);
     size 0.5–1.5, decay glideado, protección saturación en cada etapa (sin runaway/NaN/DC).
   - Topología corregida respecto al plan: los allpass difunden la **suma de combs**
     (no el predelay); L/R de-correlados con longitudes distintas (1116/1188/1277/1356 vs
     1131/1203/1293/1377).
3. **Integración** — `FxEngine.h` actualizado con setters delay/reverb (send/return/time/
   feedback/pingpong/width/mix/bypass/loop filters y predelay/decay/size/damping/width/mix/
   bypass/input filters/mode). `FxEngine.cpp` implementa `processSendReturns()`:
   `master_ = buffer` (dry), `send_[0] = buffer·delaySend_`, `send_[1] = buffer·reverbSend_`,
   `delay_.Process(send_[0], returnDelay_)`, `reverb_.Process(send_[1], returnReverb_)`,
   `out = master_ + returnDelay_·delayReturn_ + returnReverb_·reverbReturn_`. Defaults:
   sends = 0 (no cambia el audio), returns = 0.5; `SetSampleRate`/`Reset` propagan a ambos.
   `Makefile`: `DelayLine.o` y `Reverb.o` en `COMMONFILES`.
4. **Test** — `tests/test_fx_phase2_delay_reverb.py` → `DELAY_REVERB_PHASE2_OK` (puerto Q15
   fiel de ambos DSP): delay exactitud @44.1k/@48k (±1 muestra), glide alcanza el target,
   sync divisions, feedback estable (loop gain < 1), ping-pong/width, mix sin discontinuidad,
   tail independiente al hacer bypass, fórmula RT60 (≈ −60 dB en RT60), no-runaway (entrada
   caliente 1 s + 2 s silencio → decae), sin NaN/DC (saturado y cola converge), de-correlación
   estéreo, guards de fuente (0.98f, glideDelay, DC blocker, kPredelayMax, ...).
5. **Compilación MIPS real** — `mips-mti-linux-gnu-g++` syntax-check limpio de
   `FxEngine.cpp`, `DelayLine.cpp`, `Reverb.cpp` (fix aplicado: `#include <math.h>` en
   DelayLine.cpp y declaración de `recomputeGains` en Reverb.h).
6. **Benchmark host** — `scripts/bench_fxengine.sh` → `BENCH_FXENGINE_OK`:
   - `Process` bypass: **2.485 ns/callback**; con delay+reverb activos (sends/returns):
     **33.196 ns/callback** (≈0,16 % CPU host para 918 frames @ 44.1 kHz).
   - `StaticMemoryBytes` = **1.199.616 B** (buses 311.296 + DelayLine 768.000
     + Reverb 120.320). 402k callbacks → `RtViolations = 0`.
   - Números reales de la R36SX v2.6 pendientes (sección D del plan).

Verificación: en modo legacy el bypass sigue siendo 1:1 (sends = 0 por defecto), por lo que
los golden WAVs de Fase 0 y la reproducción actual no cambian (confirmado por el audit:
`GOLDEN_LEGACY_PHASE0_OK` y `FXENGINE_BYPASS_PHASE1_OK`).

### Fase 3 — RESULTADO (aplicado y verificado)
1. **ParametricEQ** — `source/sources/Application/Audio/FxEngine/ParametricEQ.h/.cpp`:
   - 3 bandas RBJ (Audio EQ Cookbook) en **transposed direct form II** con aritmética Q15:
     banda 0 low shelf, banda 1 peaking bell, banda 2 high shelf. Setters con clamp:
     frecuencia 20–20000 Hz, ganancia ±12 dB, Q 0.1–10 (shelf slope Q clamp 0.5–2).
   - `w0` clamp a 0.9π; `A = 10^(G/40)`; normalización por `a0`; salida saturada a ±1.
   - **Bypass por banda + bypass global con crossfade** one-pole y **fix Q15 real**:
     `fp_mul(0.001, diff)` trunca a 0 cuando `|diff| < ~1024`, estancando el crossfade en
     ~0.9688 sin llegar a 1.0 → macro `FX_EQ_SNAP` (salta al target si el paso trunca a 0),
     replicada en el modelo Python (`_snap`).
   - Coeficientes recalculados a **control rate** (solo cuando cambian), dependientes del
     sample rate real (`SetSampleRate`).
2. **Compressor** — `source/sources/Application/Audio/FxEngine/Compressor.h/.cpp`:
   - Detector **feed-forward por pico** (máx |L|,|R|) con **stereo link** opcional
     (enlace deganancia: ambos canales usan el mayor nivel), soft-knee, attack/release
     one-pole, makeup gain, bypass global.
   - **Tabla estática precalculada** de 4096 niveles (gain + GR) a control rate (solo se
     recalcula si cambian threshold/ratio/knee), índice `level >> 3` (Q15 → 12 bits),
     sin trig/log en el callback. Curva: `over<=0`→0; `0<over<=knee`→
     `(over+knee)^2/(2·knee)·(1/R−1)`; `over>knee`→`over·(1/R−1)`.
   - **Limitador**: soft clip cúbico final (clamp ±1) + ganancia máx 4.0 (12 dB makeup).
   - **GR meter** suavizado (one-pole 0.005) para la UI, accesible vía
     `GetCompGainReductionDb()`.
   - `StaticMemoryBytes()` = **32.768 B** (2 tablas × 4096 × 4).
   - Comportamiento realista: con attack 15 ms y seno de 1 kHz @ 0 dBFS, el envelope solo
     trackea hasta ~0.42 (−7.5 dBFS) → GR estable ≈ **−12.3 dB** (la tabla espera −13.5);
     el test valida el rango y la tabla estática por separado.
3. **Integración** — `FxEngine.h`: miembros `eq_`/`comp_`, wrappers `SetEq*`/`SetComp*`
   (bypass, banda, frecuencia, dB, Q, threshold, ratio, knee, attack, release, makeup,
   stereo link, soft clip) y `GetCompGainReductionDb()`. `FxEngine.cpp`: la cadena master
   aplica `eq_.Process(buffer)` y después `comp_.Process(buffer)` tras delay/reverb;
   ctor/`Reset`/`SetSampleRate` propagan; EQ y comp **bypass por defecto** (sin cambio de
   audio en legacy). `Makefile`: `ParametricEQ.o` y `Compressor.o` en `COMMONFILES`.
4. **Test** — `tests/test_fx_phase3_eq_comp.py` → `EQ_COMP_PHASE3_OK` (puerto Q15 fiel de
   ambos DSP): bell @1k ±6/+12 dB en ±0.5 dB, shelves ±1 dB, identidad/bypass, comp curva
   (GR −12.35 dB @ 0 dBFS), unity below-threshold, tabla estática, attack 15 ms (envelope
   frac 0.56 ≈ 0.63), stereo link (ratio 10.0), soft clip (peak = 1.0 con entrada 2.0),
   GR meter (~0 bypass), guards de fuente.
5. **Compilación MIPS real** — `mips-mti-linux-gnu-g++` syntax-check limpio de
   `FxEngine.cpp`, `DelayLine.cpp`, `Reverb.cpp`, `ParametricEQ.cpp`, `Compressor.cpp`
   (`FXENGINE_PHASE1_MIPS_SYNTAX_OK`).
6. **Benchmark host** — `scripts/bench_fxengine.sh` → `BENCH_FXENGINE_OK` (incluye
   `ParametricEQ.cpp` + `Compressor.cpp` en el g++ y fase de medición con EQ+comp activos):
   - `Process` bypass: **2.375 ns/callback**; delay+reverb: **40.138 ns**; full (EQ+comp
     activos): **52.649 ns/callback** (≈0,21 % CPU host, 918 frames @ 44.1 kHz).
   - `StaticMemoryBytes` = **1.232.384 B** (buses 311.296 + DelayLine 768.000
     + Reverb 120.320 + comp tablas 32.768). 603k callbacks → `RtViolations = 0`,
     `CompGRdB = −9.0`.
   - Números reales de la R36SX v2.6 pendientes (sección D del plan).

### Fase 4 — Comandos FX y UI R36SX
- Comandos: delay send, reverb send, delay feedback, delay time, reverb decay, reverb size, filter cutoff, compressor threshold (en las 2 columnas FX existentes; mapeo monotónico 00-FF documentado, NO convención "1=máximo").
- UI: sends DLY/RVB por pista, páginas globales delay/reverb/master, GR meter, indicador clipping, unidades ms/Hz/dB/ratio/%, presets ECO.

### Fase 4 - RESULTADO (completo: sends + comandos + UI)

**4.1 — Sends DLY/RVB por pista (hecho y verificado)**
- `Mixer.h/.cpp`: `channelDelaySend_`/`channelReverbSend_` (0..100), getters/setters/nudgers con `clampSend`, atributos XML `DELAYSEND`/`REVERBSEND` en SaveContent/RestoreContent (backwards-compatible: atributos ausentes -> 0).
- `FxEngine.h/.cpp`: `AccumulateChannelSend(channel, buffer, samplecount, delayGain, reverbGain)` limpia los buses `send_[0]`/`send_[1]` en el primer acumulador del frame (`sendsAccumulated_`) y suma con `fp_mul`; guards RT (`legacyMode_`, frames <= `FX_ENGINE_MAX_FRAMES`, channel < `FX_ENGINE_MAX_CHANNELS`, null buffer, samplecount > 0 -> `rtViolations_++`). `processSendReturns()` consume los buses y hace fallback a los sends globales si nadie acumuló (preserva comportamiento Fases 2/3).
- `PlayerChannel.cpp`: tras volumen/mute, si el canal es audible y send != 0, `AccumulateChannelSend(index_, ...)` con las ganancias del Mixer.
- Test `tests/test_fx_phase4_track_sends.py` -> `FX_TRACK_SENDS_PHASE4_OK` (aislamiento por pista, send=100 -> bus=buffer, acumulación estéreo, fallback global, legacy no acumula, guards RT, round-trip persistencia, default legacy 0, guards de fuente).

**4.2 — Comandos FX en columnas FX (hecho y verificado)**
- `CommandList.h/.cpp`: `I_CMD_DLYS` (`DLYS`), `I_CMD_RVBS` (`RVBS`), `I_CMD_DLYT` (`DLYT`), `I_CMD_DLYF` (`DLYF`), `I_CMD_RVDC` (`RVDC`), `I_CMD_RVSZ` (`RVSZ`), `I_CMD_CMPT` (`CMPT`), añadidos a `_all[]` tras `PFIN`.
- `HelpLegend.h`: display names `DSN/RSN/DTM/DFB/RDC/RSZ/CTH` y strings de ayuda con rangos.
- `SampleInstrument::ProcessCommand`: handlers por comando, mapeo monotónico del byte bajo `value&0xFF` (byte alto = speed reservado):
  - `DLYS`/`RVBS`: send del track 0..100 -> `Mixer::GetInstance()->SetChannelDelaySend/ReverbSend(channel, ...)`.
  - `DLYT`: 10..2000 ms, `DLYF`: 0..0.98, `RVDC`: 0.2..8.0 s, `RVSZ`: 0.5..1.5, `CMPT`: -60..0 dB -> setters `FxEngine::GetInstance()` (`SetDelayTimeMs`, `SetDelayFeedback`, `SetReverbDecay`, `SetReverbSize`, `SetCompThresholdDb`). Control-rate únicamente, cero asignaciones.
- Test `tests/test_fx_phase4_commands.py` -> `FX_COMMANDS_PHASE4_OK` (mapeos monotónicos con extremos exactos, byte alto ignorado, FourCC + handlers + HelpLegend en fuente).
- MIPS syntax check: `FXENGINE_FASE4_CMDS_MIPS_SYNTAX_OK` (Mixer, PlayerChannel, FxEngine, SampleInstrument).

**4.3 — UI (hecho y verificado)**
- `MixerView` gana un sistema de páginas FX (TREEFROG_FX_PAGES_V1): SELECT cicla MIX -> DELAY -> REVERB -> MASTER -> MIX.
- Página MIX: barras clásicas + readouts de send DLY/RVB por pista bajo cada barra (filas 16/17). R2 solo cicla el target de edición VOL/DLY/RVB del canal hovereado; UP/DOWN y A+UP/DOWN editan según el target (`Mixer::NudgeChannelDelaySend/ReverbSend`).
- Páginas DELAY/REVERB/MASTER: tabla `kFxParams_` (41 parámetros) — filas con etiqueta + valor en unidades naturales (ms, %, s, dB, Hz, ratio). UP/DOWN mueve la fila, LEFT/RIGHT edita, A+LEFT/RIGHT/A+UP/DOWN edición gruesa.
- Página MASTER: EQ 3 bandas completo (freq/gain/Q/enable por banda + bypass) y compresor completo (threshold, ratio, knee, attack, release, makeup, stereo link, soft clip, bypass) + GR meter en vivo.
- Getters de control-rate añadidos a `DelayLine`/`Reverb`/`ParametricEQ`/`Compressor` y readbacks en `FxEngine` (`GetDelayTimeMs`, `GetReverbDecay`, `GetEqBandFreq`, `GetCompThresholdDb`, ...) — la UI solo escribe en control-rate.
- Test `tests/test_fx_phase4_ui.py` -> `FX_UI_PHASE43_OK` (readback DSP, roundtrip Q15, consistencia tabla 9/10/22, edición sends, wiring UI, getters).
- MIPS syntax check: `FXENGINE_FASE43_UI_MIPS_SYNTAX_OK` (5 módulos DSP + MixerView).

### Fase 5 — Compatibilidad y hardening (hecho y verificado)
- **Hallazgo crítico**: `legacyMode_` (default `true`, bypass bit-idéntico al LGPT original) nunca se desactivaba en la app -> los setters de UI/comandos escribían parámetros pero `Process()` seguía en bypass. Decisión: **auto-engage al editar** (tocar cualquier parámetro FX o send desactiva legacy; volver todo a default lo re-marca).
- **Auto-engage (FxEngine.h/.cpp)**: cada setter público llama `RefreshLegacy()`; nuevos `NotifyChannelSendActive(bool)`, `RefreshLegacy()`, `AllParamsAtLegacyDefault()`, miembro `anyChannelSendActive_`. `AllParamsAtLegacyDefault()` compara todos los defaults legacy (dly time/fb 0, width/mix 1, pp/sat/byp false; reverb predelay 0, decay 1.0, size 1, damping 0.5, width 1, mode NORMAL, mix 1, byp false; sends 0, returns `fl2fp(0.5)`; EQ bypass true + bandas 100/1000/10000 Hz desactivadas; comp bypass true, thr -24, ratio 4, knee 6, atk 15 ms, rel 200 ms, makeup 0, link/softclip true).
- **Persistencia (Mixer.h/.cpp)**: `NotifyFxSends()` (recorre los 8 canales -> `NotifyChannelSendActive`) invocado desde `SetChannelDelaySend/SetChannelReverbSend/Clear`. `SaveContent` serializa `<FXMASTER>` con 41 atributos (`DLYSEND`..`CMPBYP`); `RestoreContent` lee `FXMASTER` si existe (aplica setters con `(fixed)value`) o resetea FxEngine a defaults legacy si no existe -> los proyectos LGPT clásicos siguen sonando idénticos y se auto-engran al primer edit.
- **Hardening**: los 4 DSP (`DelayLine`, `Reverb`, `ParametricEQ`, `Compressor`) compilan con `-Wall -Wextra -Werror` (`HARDEN_SYNTAX_WERROR_OK`) y pasan ASan+UBSan con harness estocástico (2000 iteraciones, valores extremos; `rtViolations=0` en los 4, `HARDEN_SANITIZERS_OK`). Corregidos 2 `-Wreorder` (listas de inicialización de `Reverb` y `Compressor`).
- **MIPS syntax check**: `MIPS_FX_SYNTAX_FASE5_OK` (5 módulos DSP con `mipsel-linux-gnu-g++`, flags buildroot 74kc/dspr2).
- **Test**: `tests/test_fx_phase5_persistence_legacy.py` -> `FX_PERSISTENCE_LEGACY_PHASE5_OK` (default-is-legacy, edit-master-param-engages, edit-channel-send-engages, return-to-default-reengages, roundtrip FXMASTER 41 params, legacy-project-default, source guards). Suite completa Fases 0-5: 10/10 OK.
- **Build + instalación**: core `lgpt_r36sx_u2523.so` compilado con toolchain MIPS real -> `BUILD_U2523_OK`; instalado en SD -> `INSTALL_U2523_OK` (backup `LGPT_BEFORE_U2523_20260802_130251`); verificado -> `VERIFY_U2523_OK` (ERRORS=0). Símbolos `RefreshLegacy`, `AllParamsAtLegacyDefault`, `NotifyFxSends`, etiqueta `FXMASTER` presentes en el core de la SD.
- Prueba prolongada R36SX: 10 min, 8 pistas, 0 underruns, presets ECO. *(queda pendiente en hardware)*

### Fase 6 — Sends por instrumento, rediseño de UI y navegación A+B=default (hecho y verificado)

**6.1 — Sends FX por instrumento (hecho y verificado)**
- `I_Instrument.h`: virtuals no-virt-pure `GetFxDelaySendOverride()`/`GetFxReverbSendOverride()` (default `0xFF` = "hereda el send per-track del Mixer") y `GetFxDry()` (default 100). `MidiInstrument` queda intacto.
- `SampleInstrument`: variables persistidas `DRY` (`SIP_DRY`, 0..100, default 100), `DLY send` (`SIP_DLY_SEND`, 0..100, default -1 = inherit) y `RVB send` (`SIP_RVB_SEND`, idem). Se serializan como PARAMs ordinarios.
- `PlayerChannel::Render`: `gain = (send% * DRY%) / 10000.0f` con `fl2fp`; el override del instrumento gana; si no hay override se hereda el send per-track del Mixer. `DRY=100` es bit-idéntico a Fase 4/5.
- Handlers `I_CMD_DLYS`/`I_CMD_RVBS`: escriben el override del instrumento Y el send per-track del `Mixer` (UI legacy + persistencia consistentes).
- Test `tests/test_fx_phase6_instrument_sends.py` -> `FX_INSTRUMENT_SENDS_PHASE6_OK` (default hereda track, DRY=100 == Fase 4, DRY escala lineal, override gana, DLYS/RVBS tocan Mixer, guards de fuente).

**6.2 — Rediseño MixerView: 5 páginas (hecho y verificado)**
- `MixerView.h`: enum `FxPage` = MIX, DELAY, REVERB, EQ, COMP (`FX_PAGE_COUNT=5`). SELECT cicla MIX -> DELAY -> REVERB -> EQ -> COMP.
- `kFxParams_` = 37 parámetros (delay 7, reverb 8, EQ 13, comp 9); eliminadas las filas globales `DLY SND/DLY RET/RVB SND/RVB RET` (los sends pasan a ser per-pista/per-instrumento y los returns quedan fijos en `fl2fp(0.5)`).
- Títulos por página: `DELAY MASTER`, `REVERB MASTER`, `MASTER EQ`, `MASTER COMP` (GR meter en COMP).
- Test `tests/test_fx_phase4_ui.py` actualizado -> `FX_UI_PHASE43_OK` (5 páginas / 37 params).

**6.3 — InstrumentFxModal eliminado (hecho y verificado)**
- Borrados `InstrumentFxModal.{h,cpp}`; el Makefile no lo compila.
- R2+A en el Mixer ya no abre un modal: salta a `VT_INSTRUMENT` con `viewData_->currentInstrument_=mixerCol_`. Hint en la página MIX: `R2+A instr`.

**6.4 — InstrumentView: campos de send + navegación A+B=default (hecho y verificado)**
- Bloque `fx sends: dry:%3d dly:%3d rvb:%3d` insertado tras `SIP_PAN` (`UIStaticField* sendLabel`).
- `A+B` resetea el campo enfocado a su default (`Variable::Reset()`), incluyendo los campos editables del InstrumentView.
- En las páginas FX de `MixerView`, `A+B` restaura la fila hovereada a su default legacy (`fxResetRow()` + columna `vdef` en `kFxParams_`, alineada con `AllParamsAtLegacyDefault` de Fase 5) — permite devolver toda la página al estado "all defaults" sin buscar a mano.
- **Conflicto resuelto**: como `A+B` y `B+A` comparten el mismo bitmask, la acción legacy "B+A cortar instrumento / limpiar tabla" se mueve a `L2+A`.
- Test `tests/test_fx_phase6_nav_ab_default.py` -> `FX_NAV_AB_DEFAULT_PHASE6_OK` (37 params / 5 páginas, defaults == `AllParamsAtLegacyDefault`, reset idempotente, guards de fuente).

---

## G. Diseño de clases / APIs (FxEngine)

```
namespace FxEngine {

  // Buses estáticos, preasignados, cero malloc
  class Buses {
    fixed dry_[CH][BUF];        // dry por canal
    fixed send_[CH][BUF];       // send por canal (delay/reverb sums)
    fixed returnDelay_[BUF*2];  // estéreo
    fixed returnReverb_[BUF*2];
    fixed master_[BUF*2];
  };

  // Delay estéreo
  class DelayLine { // circular, preasignado, interpolación
    fixed* buffer_; int cap_; int write_;
    fixed readFrac_/smoothing_; bool pingPong_; ...
    void process(stereo, param_t delayMs, fb, width, hpf, lpf, sat, bypass);
  };

  // Reverb (estrategia intercambiable: Dattorro | FDN)
  class Reverb {
    enum Type { DATTORRO, FDN4, FDN8, OFF };
    DelayLine predelay_;
    fixed diffusion_, size_, decay_ (RT60), damping_, mod_, width_, inputHP/inputLP_;
    void process(stereo, ...);
    // protección: límite de energía, guardas NaN/DC
  };

  // EQ 3 bandas
  class ParametricEQ {
    Biquad lowShelf_, bell_, highShelf_;  // Audio EQ Cookbook
    void set(sampleRate, hz, db, q);      // solo en control-rate
    void process(stereo, bypass);
  };

  // Compresor/limitador master
  class Compressor {
    bool link_; float thr_, ratio_, knee_, attack_, release_, makeup_;
    float grMeter_;  // para UI
    void process(stereo, bypass);
  };

  // Orquestador
  class FxEngine {
    Buses buses_; DelayLine delay_; Reverb reverb_;
    ParametricEQ eqReturns_, eqMaster_; Compressor comp_; SoftClip softClip_;
    bool legacyMode_;  // bypass total = 1:1
    void process(const fixed** channels, int nch, int samples, int sampleRate);
  };
}
```

API de integración:
- `FxEngine::process()` se llama en `AudioMixer::Render` (AudioMixer.cpp:71-181) tras mezclar canales.
- `legacyMode_` preserva la ruta original (golden tests).
- Control/UI solo escribe parámetros (control-rate); el DSP nunca recibe eventos RT bloqueantes.

---

## H. Matriz de pruebas

| Categoría | Prueba | Criterio aceptación |
|-----------|--------|---------------------|
| Básicas | Impulso | Respuesta finita, sin artefactos |
| Básicas | Silencio | Salida = 0, sin zumbido |
| Básicas | DC step | Decae según filtros, sin NaN |
| Básicas | Seno | THD dentro de tolerancia |
| Básicas | Ruido / sweeps | Sin aliasing fuera de banda esperada |
| Robustez | Cambios abruptos de parámetro | Sin clics (smoothing/crossfade) |
| Robustez | NaN/Inf inputs | Protegidos, no propagan |
| Delay | Exactitud @44.1k y @48k | ±1 muestra |
| Delay | Feedback seconds | Estable, loop gain <1 |
| Delay | Ping-pong/width/sat | Sin overflow Q15 |
| Reverb | RT60 vs objetivo | Tolerancia documentada |
| Reverb | Densidad / correlación estéreo | Métricas definidas |
| Reverb | Runaway | Nunca crece indefinidamente |
| EQ | Respuesta vs spec | ±0.5 dB |
| Comp | Curva de ganancia | Tolerancia documentada |
| Comp | Attack/release | Forma correcta |
| Comp/lim | Ganancia máx feedback | <1 para evitar runaway |
| Integración | Golden legacy WAVs | Sin diferencias en modo legacy |
| Persistencia | Migración ida/vuelta | Defaults legacy conservados |
| Multitrack | 8 pistas simultáneas | 0 underruns 10 min ECO |
| Plataforma | local / ALSA / USB 48k | Coherente en todas |
| Plataforma | Prueba prolongada R36SX v2.6 | 0 underruns, memoria estática |
| Rendimiento | 0 allocs/syscalls en callback | Verificado con contador |
| Compat | Mapas hex→decimal | Curvas monotónicas 00-FF |

---

## Reglas transversales
- Cero allocs/disco/logging/mutex/syscalls en callback de audio.
- Coeficientes a control rate. Sin `%` en loops críticos. No asumir FPU.
- Sanitizers en dev; tests con -O2; warnings-as-errors en módulos nuevos.
- Atribuciones de licencia (MIT): Mutable Instruments Clouds, DaisySP, Signalsmith Basics, Audio EQ Cookbook → archivo ATTRIBUTIONS.md.
- No copiar código del tracker M8.
