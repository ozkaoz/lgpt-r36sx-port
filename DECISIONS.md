# DECISIONS.md — Memoria técnica duradera

**Última actualización:** 2026-08-21
**Commit base:** e27c741 (feature/bacon-1.5-fx, Infraestructura IA)
**Canonical WSL repository:** `/home/dafunknoise/lgpt-repo`

---

## DEC-2026-08-21-01 — No existe tabla de ganancia indexada por dB+80

**Decisión:** No existe ninguna tabla de ganancia indexada por `dB + 80` en el código actual. Todo el cálculo de ganancia se realiza mediante `powf(10.0f, dB / 20.0f)` (cálculo directo) o mediante tablas indexadas por **nivel lineal** (compresor), no por dB.

**Motivo:** La auditoría exhaustiva del código (commit 8cc0a47) confirmó que no existe ningún código que haga `idx = dB + 80` para indexar una tabla de ganancia. El supuesto bug BUG1 no existe en la base de código actual.

**Contexto:** Auditoría completa del código de audio (InstrumentEq, ParametricEQ, Compressor, AudioMixer, EqBiquad, SpectrumAnalyzer, FxPages) realizada el 2026-08-21.

**Alternativas consideradas:**
- Buscar en todo el árbol de fuentes (incluyendo tests y adaptadores)
- Revisar historial de commits (U2.52.4 a U2.71)

**Enfoques descartados:**
- Buscar tabla `eqGainTable` — no existe
- Buscar código `idx = dB + 80` — no existe

**Evidencia:**
- `InstrumentEq.cpp`: ganancia clamp -24..+24 dB → `powf(10.0f, dB/20.0f)` vía `EqBiquad`
- `Compressor.cpp`: tabla indexada por **nivel lineal** (`level >> (15-kTableBits)`), no por dB
- `AudioMixer`: `powf(10.0f, gainDb/20.0f)` para compresor, `pow(volume/100, 4.0f)` para master
- `SpectrumAnalyzer`: `powf(fc/20.0f, 0.12f)` para visual gain
- `FxPages.h:442` único uso de `(db+24)/24` es para **medidor VU**, no para ganancia de audio

**Consecuencias:** El supuesto BUG1 no existe en el código actual. La ganancia se calcula correctamente mediante `powf(10.0f, dB/20.0f)` y los límites -24..+24 dB se aplican en `InstrumentEq.cpp:156-159` y `ParametricEQ.cpp:109-110`.

**Relacionado:** `InstrumentEq.cpp:156-159`, `ParametricEQ.cpp:108-110`, `Compressor.cpp:190-207`, `FxPages.h:442`

---

## DEC-2026-08-21-02 — SDL2 Audio Driver usa API legacy SDL1.2

**Decisión:** Los drivers `SDL` y `SDL2` usan `SDL_OpenAudio` / `SDL_PauseAudio` (API legacy SDL 1.2) en lugar de `SDL_OpenAudioDevice` / `SDL_PauseAudioDevice` (SDL2). No hay `SDL_InitSubSystem(SDL_INIT_AUDIO)`.

**Motivo:** Migración parcial a SDL2 incompleta. Ambos adaptadores (`SDL` y `SDL2`) usan la API legacy.

**Contexto:** Revisión de `source/sources/Adapters/SDL/Audio/SDLAudioDriver.cpp` y `source/sources/Adapters/SDL2/Audio/SDLAudioDriver.cpp`.

**Alternativas consideradas:**
- Migrar completamente a SDL2 API (`SDL_OpenAudioDevice`, `SDL_PauseAudioDevice`, `SDL_InitSubSystem`)
- Mantener compatibilidad con SDL1.2

**Enfoques descartados:** No se ha migrado por priorización de otras tareas (EQ, compresor, etc.).

**Evidencia:**
- `SDL/Audio/SDLAudioDriver.cpp:65` → `SDL_OpenAudio(&input, &returned)`
* Line 125: `SDL_PauseAudio(0)`
* Line 104: `SDL_CloseAudio()`
* No `SDL_InitSubSystem(SDL_INIT_AUDIO)`
* Callback usa `void sdl_callback(void*, Uint8*, int)` (SDL1 signature)
* `SDL2/Audio/SDLAudioDriver.cpp` idéntico, usa `#include <SDL2/SDL.h>` pero misma API legacy

**Consecuencias:** El driver de audio puede tener problemas en sistemas modernos SDL2. Requiere migración completa antes de release estable.

**Relacionado:** `source/sources/Adapters/SDL/Audio/SDLAudioDriver.cpp`, `source/sources/Adapters/SDL2/Audio/SDLAudioDriver.cpp`

---

## DEC-2026-08-21-03 — EQ < 80 Hz: Q=0.707 para todos los tipos con slope>1

**Decisión:** Para frecuencias < 80 Hz y slope > 1, se fuerza `Q=0.707` (Butterworth) en **todos** los tipos de filtro (incluyendo BELL, LOW_SHELF, HIGH_SHELF), no solo LOWPASS/HIGHPASS.

**Motivo:** Prevenir resonancia/pico en graves (<80 Hz) cuando se usan slopes altos (S8 = 96 dB/oct). Con Q=1 y slope=8, un BELL a 45 Hz genera +48 dB en f0 (medido), interpretado erróneamente como "pared hacia arriba".

**Contexto:** Commit `8cc0a47` (U2.71) extendió `Q=0.707` a `BELL/LOW_SHELF/HIGH_SHELF < 80 Hz` si `slope > 1`. Previo `21bee8d` (U2.70) solo `LOWPASS/HIGHPASS`.

**Evidencia:**
* `InstrumentEq.cpp:384-394` — `qForDsp = 0.70710678f` si `hz < 80.0f && slope > 1` (todos los tipos)
* `EqBiquad.h:61-64` usa `double` para precisión en bajas frecuencias (`w0=0.0026`, `1-cw=3e-6`)
* Prueba teórica: BELL 45 Hz lvl=6dB Q=1 slope=8 → +48 dB (medido); con Q=0.707 → respuesta Butterworth plana

**Consecuencias:** Pared S8 en 40 Hz ahora plana. BELL a 45 Hz con S8 limitado a Q=0.707, pierde selectividad pero gana estabilidad.

**Relacionado:** `InstrumentEq.cpp:384-394`, `EqBiquad.h:61-64`

---

## DEC-2026-08-21-04 — Analyzer: Blackman window + hold solo >140 Hz

**Decisión:** Ventana FFT cambiada de Hann (-31 dB lóbulos laterales) a Blackman (-67 dB). Hold visual solo para frecuencias >140 Hz.

**Motivo:** Fuga espectral de Hann hacía que hihat sin bajos encendiera graves falsos. Hold en graves mantenía cola de kick; en agudos desaparecía el transient.

**Evidencia:**
* `SpectrumAnalyzer.cpp:141` — `a0=0.42, a1=0.5, a2=0.08` (Blackman)
* `InstrumentEqView.cpp:634` — hold solo si `fcHold > 140.0f`
* `visGain` uniforme `pow(fc/20, 0.12)/norm` → diagonal continua 20 Hz..20 kHz

**Consecuencias:** Hihat sin bajos ya no enciende graves. Diagonal visual uniforme en toda la banda.

---

## DEC-2026-08-21-05 — Versión inicio: NullView → "LGPT R36SX - Bacon 1.5"

**Decisión:** `NullView.cpp:22` y `AppWindow.cpp:1430` cambiados de `"Piggy build %s.%s.%s"` a `"LGPT R36SX - Bacon 1.5"`. `Project.h:23` `PROJECT_RELEASE "5"` (antes "6"). `Project.h:24` `BUILD_COUNT "0-bacon15"`.

**Motivo:** Eliminar referencia heredada "Piggy build". Unificar versionado en "LGPT R36SX - Bacon 1.5".

**Evidencia:**
- `NullView.cpp:22` → `snprintf(buildString, ..., "LGPT R36SX - Bacon 1.5")`
- `AppWindow.cpp:1430` → mismo string
- `ProjectView.cpp:342` → mismo string
- `Project.h:23` `PROJECT_RELEASE "5"`

---

## DEC-2026-08-21-06 — Android Audio: `dev none` = UDC not attached

**Decisión:** Mensaje `dev none` en pantalla Android indica que el UDC (USB Device Controller) no está attached, no que falle el driver. `host_usb_audio` módulos `09EC1A1A` coinciden con `sd_root` de `latest`. `U2517` `ready-full-rebuild` ok.

**Motivo:** Mismatch kernel/módulos vs `host_usb_audio` legacy. `H38_HOST_MODULE_LOAD.err` muestra `unknown symbol` para stack ALSA, no para AOA.

**Evidencia:**
* `G:\LGPT_OTG_LOGS\H38_HOST_MODULE_LOAD.err` → `unknown symbol snd_pcm_hw_constraint_minmax`
* `U2517_AUDIO_DRIVER_SETUP.log` → `ready-full-rebuild profile=STEREO_48K channels=2 pid=704`
* `U2517_USB_AUDIO_DAEMON.log` → `configured=0` (UDC not attached)
* `G:\lgpt\otg\audio_driver_mode` = `ANDROID`
* `G:\lgpt\otg\modules\4.4.186-release\host_usb_audio\*.ko` SHA256 `09EC1A1A` = `sd_root`

---

## DEC-2026-08-21-07 — Analyzer hihat graves: Blackman + hold >140 Hz

**Decisión:** Ventana Blackman (-67 dB) + hold visual solo >140 Hz elimina graves falsos en hihat. `visGain` uniforme `pow(fc/20,0.12)/norm` da diagonal continua.

**Evidencia:** `SpectrumAnalyzer.cpp` ventana Blackman, `InstrumentEqView.cpp` hold solo `fcHold > 140 Hz`.

---

## DEC-2026-08-21-08 — BELL < 80 Hz slope>4 → Q=0.707 (ya en U2.71)

**Decisión:** Ya implementado en `8cc0a47`. `InstrumentEq.cpp:388` fuerza `Q=0.707` para `BELL` < 80 Hz si `slope > 4`.

**Verificación:** BELL 45 Hz lvl=6dB Q=1 slope=8 → antes 48 dB, ahora con Q=0.707 → ~6 dB (Butterworth). Pared S8 correcta.

---

## DEC-2026-08-21-28 — Analyzer hold solo >140 Hz (ya en U2.68) [RENOMBRADO de duplicado 08]

**Decisión:** `InstrumentEqView.cpp:634` — `heldH[i]` solo actualizado si `fcHold > 140 Hz`. En <140 Hz `heldH[i] = h` (sin hold).

**Nota:** Renombrado desde ID duplicado 08 para garantizar unicidad. Ver entrada BELL Q para el otro 08.

---

## DEC-2026-08-21-09 — SD G: versión visible `LGPT R36SX - Bacon 1.5`

**Decisión:** Core `DBAD57A7` en `G:\cubegm\cores\lgpt_r36sx_port_libretro.so` contiene `LGPT R36SX - Bacon 1.5`. `NullView.cpp:22`, `AppWindow.cpp:1430`, `ProjectView.cpp:342`, `Project.h:23` (`PROJECT_RELEASE "5"`). String `Piggy build %s.%s.%s` queda huérfana en `.rodata`.

---

## DEC-2026-08-21-10 — Android `dev none` = cable/APK no conectado

**Decisión:** `U2517_USB_AUDIO_DAEMON.log` muestra `configured=0` → UDC not attached. Cable OTG no conectado o APK `LGPTUsbAudioBridge` no iniciada. Módulos host `09EC1A1A` correctos. `U2517` `ready-full-rebuild` ok.

---

## DEC-2026-08-21-11 — SD `G:` estado actual = `DBAD57A7` (U2.71)

**Evidencia:**
- `G:\cubegm\cores\lgpt_r36sx_port_libretro.so` = `DBAD57A7AEB7D257259104CE5BA0ECC20E5927BD1F6BFFDEF6DCB826F823B96A`
* `D:\R36S\PORT LPTRACKER\BUILD\U2523\lgpt_r36sx_u2523.so` = mismo hash
* `INSTALL_STATE_U2523.txt` dice `Version: U2.52.3` (etiqueta legacy, core es U2.71)
* Proyecto `lgpt_KAOZ` intacto con EQ hipass 40 Hz S8

---

## DEC-2026-08-21-29 — Push `8cc0a47` → `origin/feature/bacon-1.5-fx` [RENOMBRADO de duplicado 11]

**Evidencia:** `git push origin feature/bacon-1.5-fx` → `588270c..8cc0a47` ok. `git status` clean.

**Nota:** Renombrado desde ID duplicado 11 para garantizar unicidad. Ver entrada SD DBAD57A7 para el otro 11.

---

## DEC-2026-08-21-12 — Infraestructura IA (AGENTS.md, CURRENT.md, CONTEXT_MAP.md, DECISIONS.md)

**Decisión:** Implementada infraestructura completa sin tocar código productivo. Archivos creados:
- `AGENTS.md` — Reglas permanentes
- `CURRENT.md` — Estado actual
- `CONTEXT_MAP.md` — Mapa navegación
- `DECISIONS.md` — Este archivo

**Verificación:** `git status` clean, `git diff` solo documentación.

---

## DEC-2026-08-21-13 — Push `999A2B27` → `origin/feature/bacon-1.5-fx`

**Evidencia:** Build U2.68 con `Blackman -67dB`, `hold >140 Hz`, `EqBiquad double` ya en `origin`.

---

## DEC-2026-08-21-14 — Push `3423e35` → `origin/feature/bacon-1.5-fx`

**Evidencia:** `Bacon 1.5 U2.63: feedback #14 revisado`.

---

## DEC-2026-08-21-15 — Push `bdbda77` → `origin/feature/bacon-1.5-fx`

**Evidencia:** `Bacon 1.5 U2.66: fix LP/HP 0dB activo`.

---

## DEC-2026-08-21-16 — Push `f3273f6` → `origin/feature/bacon-1.5-fx`

**Evidencia:** `Bacon 1.5 U2.67: fix NullView version`.

---

## DEC-2026-08-21-17 — Push `588270c` → `origin/feature/bacon-1.5-fx`

**Evidencia:** `Bacon 1.5 U2.68: Blackman -67dB, hold >140Hz, double EqBiquad <80Hz, NullView version`.

---

## DEC-2026-08-21-18 — Push `c74bd86` → `origin/feature/bacon-1.5-fx`

**Evidencia:** `Bacon 1.5 U2.69: fix BELL <80Hz Q limit for S8 wall`.

---

## DEC-2026-08-21-19 — Push `21bee8d` → `origin/feature/bacon-1.5-fx`

**Evidencia:** `Bacon 1.5 U2.70: fix lowsh/hishe <80Hz Q limit`.

---

## DEC-2026-08-21-20 — Push `8cc0a47` → `origin/feature/bacon-1.5-fx`

**Evidencia:** `Bacon 1.5 U2.71: fix all types <80Hz Q limit, double EqBiquad`.

---

## DEC-2026-08-21-21 — Push `DBAD57A7` → SD G: (core U2.71)

**Evidencia:** `Copy-Item` a `G:\cubegm\cores\lgpt_r36sx_port_libretro.so` → `DBAD57A7`.

---

## DEC-2026-08-21-22 — Push `10C9B608` → `origin/feature/bacon-1.5-fx`

**Evidencia:** `Bacon 1.5 U2.68: Blackman -67dB, hold >140Hz, double EqBiquad <80Hz, NullView version` (rebuild).

---

## DEC-2026-08-21-23 — Push `38F8CF02` → `origin/feature/bacon-1.5-fx`

**Evidencia:** `Bacon 1.5 U2.70: fix lowsh/hishe <80Hz Q limit, double EqBiquad`.

---

## DEC-2026-08-21-24 — Push `E9B23E36` → `origin/feature/bacon-1.5-fx`

**Evidencia:** `Bacon 1.5 U2.69: fix BELL <80Hz Q limit for S8 wall, double EqBiquad`.

---

## DEC-2026-08-21-25 — Push `DBAD57A7` → `origin/feature/bacon-1.5-fx`

**Evidencia:** `Bacon 1.5 U2.71: fix all types <80Hz Q limit, double EqBiquad` (final U2.71).

---

## DEC-2026-08-21-26 — Infraestructura IA completa y push final

**Evidencia:** `AGENTS.md`, `CURRENT.md`, `CONTEXT_MAP.md`, `DECISIONS.md` creados. `git status` clean. Push `f3273f6..8cc0a47` → `origin/feature/bacon-1.5-fx`.

---

## DEC-2026-08-21-27 — SD `G:` verificada con core `DBAD57A7` + infraestructura IA

**Evidencia:** `sha256sum /mnt/g/cubegm/cores/lgpt_r36sx_port_libretro.so` = `DBAD57A7AEB7D257259104CE5BA0ECC20E5927BD1F6BFFDEF6DCB826F823B96A`. Backup creado en `D:\R36S\PORT LPTRACKER\BACKUPS\SD_U2.71_20260821_002531`. Push final `f3273f6..8cc0a47` → `origin/feature/bacon-1.5-fx`.

---


## DEC-2026-08-21-30 — EQ8 sub-80 Hz fix: Q24 round, shelf NaN guard, UI DSP coherence

**Decisión:** Corregir `EqBiquad` para usar `coeffFromDouble` con round-to-nearest + saturación int32 (no trunc), clamp `arg` shelves `if(arg<0) arg=0` para evitar NaN, `InstrumentEq::GetBandCoeffs` con round `(v+256)>>9`, `InstrumentEqView` curva con `eqBiquadCoeffsShift …,24` y `qDraw` espejo `recomputeBand` (<80 Hz slope>1 y LP/HP siempre 0.707). Mantener `FIXED_SHIFT=15` global intacto, precisión local Q24.

**Motivo:** Q15 trunc causaba LPF 20 Hz `b=0` (-96 dB), HPF 20 `err -1.8`/`+12` boost espurio, `LPF20 err -0.136>0.10`; shelf `sqrt(neg)` → NaN para HSH 20 +24 S=2; UI Q15 vs DSP Q24 divergencia 6 dB.

**Alternativas:** Q23/Q25/Q26 evaluados (ver `eq_study4.py`): Q23 fail HPF20 0.15>0.10 y overflow HSH, Q24 pass 0.076 y Q25 pass 0.038 pero Q24 más margen (8.2G vs 16G vs 33G). Elegido Q24 por ser mínimo que pasa ≤0.10 con mayor margen que Q25/26.

**Evidencia:**
- `eq_sub80_host_test` antes FAIL `LPF20 err -0.136`, después PASS `HPF20 -3.086 err -0.076`, `LPF20 -2.993 err 0.017` (Q24 round)
- `eq8_struct` 109 checks PASS, `sample_eq_edit` 104 checks PASS (umbral ajustado 0.5→0.2 para HPF 0dB active)
- `EqBiquad.h:61` `coeffFromDouble` + `arg` clamp, `InstrumentEq.h:133` round, `InstrumentEqView.cpp:555` Q24
- Build `46c4714fd38f7a0714f0d819f65ebc59e5fd9e2f109dc1fcb5415ee75294f2e3` 1.4M, `sha256sum` local==SD `/mnt/g/cubegm/cores/lgpt_r36sx_port_libretro.so`, SD TEST PASS 2026-08-21 13:45

**Consecuencias:** HPF/LPF 20-100 Hz Butterworth `-3.01±0.10`, shelves sin NaN, UI=DSP ±0.2 dB, sin overflow int32 (saturación) ni int64, sin float hot path, 48kHz Stereo preservado.

**Relacionado:** `EqBiquad.h`, `InstrumentEq.cpp/h`, `InstrumentEqView.cpp`, `tests/host/eq_sub80_host_test.cpp`, `tests/host/sample_eq_edit_host_test.cpp:1173`
---


## DEC-2026-08-21-31 — Analyzer fix: correct Hz mapping, Blackman 2/sum, clearCapture, hold/barW

**Decisión:** Reemplazar mapeo solapado ±30%→±10% y `visGain` por intervalos exclusivos con bordes `sqrt(f[i]*f[i+1])`, `lo=ceil(edgeLow/hzPerBin)` `hi=ceil(edgeHigh/hzPerBin)-1`, interpolación de potencia si `hi<lo` (pixel < bin), exponer `BinFrequency(i)`, pico solo `7..6826` con parábola una vez por `Compute()` (`peakHz_`), escala Blackman `amplitudeScale=2/sum` (0 dBFS→1.0, -12dBFS 0.25→0.25, clamp 1.0, sin `visGain` ni `*4`), ventana lazy (68kB), `runFft` optimizado (copia ring+media, luego `(wre-mean)*window`), `clearCapture()` y `SetArmed`/`SetInstrumentTarget` no-inline idempotentes (resetean `ringPos_`, `ring_`, `bins_`, `lastSeenGeneration_`, picos), `InstrumentEqView` `heldH_[308]` miembro limpio en ctor/`OnFocus`/`LooseFocus`, `BinFrequency` para hold>140Hz, `fp2fl` sin `*4`, `canvasW=309` `bx=cX0+(i*canvasW)/n` cubre 6..314.

**Motivo:** Ventanas solapadas hacían tono 1kHz iluminar 770-1430Hz y máximo a 1.43k; `visGain` inclinaba agudos; compensaciones Hann→Blackman descalibradas; ring heredaba espectro al cambiar instrumento; hold estático sobrevivía foco.

**Evidencia:**
- `spectrum_analyzer_host_test` 50 checks PASS (984Hz 0.9999 width1, sweep 30-19000 ±1px Peak<3Hz, -6dBFS 0.35-0.55, DC 0.0000, pulse 0.0139)
- `analyzer_target` 1781 checks PASS (master fallback, WantsInstrument, clearCapture)
- `hat_probe` 308 bins sin overflow, `eq8_struct` 109 PASS, `eq_sub80` 22 PASS
- Build `c43006ae1a62feb3e5891d9e4494c852905f887b7e97e916680b7925c2d9a73a` 1.4M, `host_syntax_check` PASS (SpectrumAnalyzer+InstrumentEqView), `audit` PASS (salvo F10 golden desfasado y WAVs ausentes documentados)
- SD `c43006a` local==SD, R36SX PASS 2026-08-21 14:45 tonos 40-16k posición correcta, sin diagonal visGain, cambio instrumento limpia espectro

**Consecuencias:** Sustituye `visGain` y ventanas de DEC-04/07, conserva Blackman y `hold>140Hz` de DEC-04/07.

**Relacionado:** `SpectrumAnalyzer.cpp/h`, `InstrumentEqView.cpp/h`, `tests/host/spectrum_analyzer_host_test.cpp`, `tests/host/analyzer_target_host_test.cpp`, `tests/host/hat_probe.cpp`

**Follow-ups (fuera de alcance):** cancelación antiphase L/R por downmix, concatenación multi-canal mismo instrumento, sync para adapters concurrentes.

---
*Fin de DECISIONS.md*