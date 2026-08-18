# Changelog

## Update: Bacon 1.5 U2.52.3 - EQ8 fullscreen + audición aislada + guard RBJ

- **Estado**: actualización de la pre-release Bacon 1.5 (Synths + EQ8).
  Cierra la spec de 10 puntos: EQ8 fullscreen, audición aislada, analyzer
  dirigido, sintetizadores preescuchables.
- **EQ8 fullscreen** (`Application/UI/Views/InstrumentEqView`): reemplaza el
  modal `InstrumentEqModal` (borrado). La curva de respuesta se dibuja con
  `GetBandCoeffs` — los MISMOS coeficientes que procesa el DSP (curva ==
  sonido). `freqToX/xToFreq` solo en PLATFORM_TREEFROG.
- **Audición aislada** (`Application/Audio/AuditionService`): el preview se
  rutea a un canal/bus propios (insertado en el árbol del master,
  `SetClipBypass`); suena aunque la pista esté muteada o a volumen 0.
  `Player::StopPreview` toca SOLO la audición — los 8 PlayerChannel nunca se
  detienen (test estático `test_fx_phase19_audition_isolated.py`); el
  desarm del hilo de audio está guardado por `!isRunning_`.
- **SpectrumAnalyzer dirigido** (`FeedChannel` + `SetArmed` +
  `SetTargetInstrument`): tap común post-EQ/pre-gain en `PlayerChannel::Render`
  y en `AuditionChannel::Render`; graba solo si está armado y el instrumento
  es el objetivo. Test host `analyzer_target_host_test` (54 checks).
- **Guard de estabilidad RBJ** (`Application/Audio/EqBiquad.h`): el peaking
  de la receta RBJ DIVERGE con boost en baja frecuencia — verificado en
  simulación: +6 dB @1 kHz Q1, +2 dB @250 Hz Q1 y +12 dB @80 Hz Q1 explotan
  (polo real fuera del círculo, P(1) < 0). El boost queda capeado al 90% del
  valor marginal `A = sw/(sw - 4Q(1-cw))`; aplica a InstrumentEq y a
  ParametricEQ. Cubierto por el test 9 de `eq8_struct_host_test` (31 checks).
- **Estados biquad por banda/canal** (`InstrumentEq`): la cascada ya no
  comparte estado — antes el orden de las bandas cambiaba el sonido (~23.038
  LSB medidos en el swap; ahora ~471 LSB de redondeo).
- **Smoothing con snap exacto** (`InstrumentEq.cpp`): el paso `d>>6` se
  quedaba a 63 LSBs del objetivo sin limpiar el flag; ahora se ancla el último
  residuo sub-2^-6 y la convergencia es exacta en una pasada.
- **Build 100% limpio**: `DIAGNOSTIC_GATE=0_ERRORS_0_WARNINGS`, `BUILD_U2523_OK`.
  Core `097bd4d36fb24461f83428ec007378a68e3baa6bcd291e49fc05dec209736432`.
  Regresión completa: `AUDIT_CLEAN_MAIN_U2523_OK`, `F10_BASELINE_OK`, host
  bass 45 + piano 46 + eq8_struct 31 + analyzer_target 54 checks OK.

## Release: Bacon 1.2 U2.72 - Mixer (final) - estabilización total 48k stereo

- **Estado**: release final del port `stabilize-bacon-1.2.1` (rama
  `stabilize-bacon-1.2.1`), ABI7, audio 48 kHz stereo en los cuatro modos
  (Local / Windows / Android / Sampler SP404MKII). Prueba de campo completa
  superada: switches directos repetidos entre los cuatro modos y sesiones
  continuas sin crash, sin stall del Sampler y sin detour manual por Android.
- **U2.72 H43** (`7df42da`):
  - Core: `signal(SIGPIPE, SIG_IGN)` en `retro_init` - el core moría al
    entrar a Sampler porque el daemon SP404 (único lector del FIFO
    `O_WRONLY|O_NONBLOCK`) se eliminaba durante el cambio de modo y el write
    disparaba SIGPIPE. Ahora el cambio SP404->Local/Windows/Android no tumba
    el core.
  - Bridge: tracking del FIFO abierto (`g_fifo_open_path`) y
    `fifo_compatible_with_mode()`; el fast-apply del cambio de driver solo se
    ejecuta si el FIFO abierto es el correcto; si no, cierra el FIFO y fuerza
    el apply completo de perfil. Elimina el bloqueo SP404->Windows que
    exigía el detour por Android.
  - Daemon SP404: tope de corrección ASRC 30.000->1.200 ppm (el pitch
    audible provenía del control golpeando +/-1% al barrer los bursts del
    core), EMA 15/16 del backlog de control (filtra el "peloteo" del ring),
    hold floor de 2.400 frames en `asrc_prepare_period` (nunca drena el ring
    a cero) y bookkeeping real del REENUM con salida del daemon tras 8
    intentos fallidos (`SP404_REENUM_EXHAUSTED`) liberando el rol musb host
    para que el supervisor reintente por backoff normal.
  - El pitch residual del modo Sampler (~+0,3%) es inherente al reloj
    ADAPTIVE del SP404 y se compensa con el ASRC (inaudible).
- **U2.71 H39+H40/H41/H42** (`d1c36c9`): presupuesto wall-clock de la SD,
  staging de escrituras parciales (sin escrituras a SD en runtime), micro-ASRC
  PI en el daemon Windows (`r36s_u241_usb_audio_io`), y el par de fixes de
  ASRC de Sampler.
- **U2.70** (`713913f`): ASRC FIR16 Lanczos-8 (tabla limpia, fila 79==81
  corregida) + selector `ASRC_FIR_TAPS` A/B en ambos drivers.
- **U2.69** (`be8cdcb`): ASRC drift fix para Sampler/SP404 (sin bypass,
  límites ampliados, integral 3->12, backlog 2.400->3.600) + sync del payload
  Bacon-1.2.

## Update: Bacon 1.2 U2.69 - ASRC drift fix (Sampler / SP404MKII)

- **Estado**: actualización de la pre-release Bacon 1.2 (Mixer Dev).
  Elimina los tirones audibles (~2/s) que producía el daemon
  `r36s_sp404_host_audio_io` en el modo Sampler con SP404MKII.
- **Causa raíz**: el ASRC usaba un bypass passthrough 1:1 cuando ambas tasas
  eran 48 kHz (`ASRC_PASSTHROUGH_NOW`), con step fijo = 1.0. El reloj real del
  fifo del core (~47.784 f/s) difiere del device (48.000 f/s) en ~4.500 ppm;
  sin corrección el backlog se drenaba y el daemon insertaba períodos de 480
  frames en cero (10 ms de silencio) cada ~2.2 s.
- **Fix**: el lazo ASRC P/I (FIR8 160 fases + control proporcional/integral de
  backlog) ahora se ejecuta SIEMPRE, sin bypass. Límites ampliados de ±1.200
  ppm a ±10.000 ppm (cubren la deriva real), ganancia integral 3->12
  (convergencia ~3-4 s), backlog objetivo 2.400->3.600 frames. Los tirones
  desaparecen y la corrección resultante (~0,45 % de resampleo) es inaudible.
- **Fuente/daemon**: `device/r36s_sp404_host_audio_io.c` - SP404 daemon
  `810bdc53b6f712193285d490db15fffabb28ddffaeaf992e1d8949be01d85844`.
- **Supervisor**: `otg_h37_host_runtime_supervisor.sh` ahora usa `cp -f` en el
  flush de logs (antes `cp -u` nunca sobrescribía por el mtime 1970 vs 1980 de
  FAT sin RTC) - logs frescos llegan siempre a `LGPT_OTG_LOGS/`.
- **Core 2CH FIFO FIX U2.68**: `c1ffe58977bcf48a643b2938b59c0c247812ae4f4a96ab9bb6c493c370ed17cd`.
- **Label**: `H38.7_ABI7_BACON12_MIXER_DEV_2CH_FIFO_FIX_U2.69_ASRC_DRIFT_FIX`.
## Update: Bacon 1.2.1 U2.52.7 - Per-instrument 8-Band EQ + Live Spectrum

- **Estado**: actualización de la pre-release Bacon 1.2.1 (Chopper UAF
  Hardening). Añade un EQ gráfico de 8 bandas por instrumento de sample y un
  analizador de espectro en vivo sobre el mismo modal.
- **EQ por instrumento** (`Application/Audio/InstrumentEq`): 8 bandas
  configurables por sample instrument con respuesta RBJ (bell, low/high shelf,
  low/high pass, notch), bypass global y máscara de bandas por pad.
  Parámetros expuestos a scripting vía `SetVariable`.
- **Analizador de espectro** (`Application/Audio/SpectrumAnalyzer`): FFT por
  ventana simple sobre el buffer de la voz, alimenta el overlay del EQ en
  vivo. Corregido el wrap del buffer circular en `runFft()`.
- **Interfaz** (`Application/Views/ModalDialogs/InstrumentEqModal`): modal
  con grid de bandas, curva de respuesta calculada con RBJ y espectro
  superpuesto; control por pads/zones, byte EQ 8-B en la vista de instrumento
  (`SIP_EQEN`) y máscara de bandas. Integrado en `AppWindow` vía
  `TreeFrogInstrumentEqOverlayDraw`.
- **Cache de EQ coherente** (`SampleInstrument::syncInstrumentEq`): el
  acumulador `eqCache_` se ajusta al tamaño real del array de parámetros
  (34) para que el fingerprint coincida y el cambio de bandas invalide
  correctamente el filtro.
- **Fix formato** (`InstrumentView.cpp`): `"EQ 8-B:%d"` en lugar del `%s`
  incorrecto para el valor INT (UB).
- **Build 100% limpio**: `DIAGNOSTIC_GATE=0_ERRORS_0_WARNINGS`. Core
  `11790c46940fc3d6ca924ad12235c1210e0c8e1af318038458cbe832ca4e5688`.

## Release: Bacon 1.2.1 U2.52.6 - Import Fix + Legacy Folder Cleanup

- **Estado**: hotfix de instalación sobre Bacon 1.2.1 (Chopper UAF Hardening).
  Resuelve el `Import failed` silencioso y limpia las carpetas legacy que ya no
  se usan en el layout del SD.
- **Import nunca más falla en silencio** (`SamplePool::ImportSample`): si la
  carpeta de samples del proyecto está ausente (p. ej. tras borrar el árbol del
  proyecto externamente), el import la recrea con semántica `mkdir -p`
  componente a componente y lo registra en `Trace::Log("ImportSample", ...)`.
  Cuando el `Open(dst, "w")` falla, ahora se emite un
  `Trace::Error("Failed to open project sample %s for writing ...")` con errno,
  en lugar de devolver un `-1` mudo que el frontend mostraba como
  "Import failed" sin ningún rastro.
- **Eliminadas las carpetas legacy `lgpt/samplelib` y `lgpt/project`**: el
  launcher (`device/lgpt_launcher_u241.sh` y `sd_root/cubegm/lgpt`) ya no las
  provisiona en cada boot; el core solo usa `SAMPLELIB` (`/mnt/sdcard/lgpt/
  samples`, que sí se crea) y las canciones viven en `lgpt/projects`.
  `SAMPLE_LIB` (fallback del core) y `FxPrinter::impulse_dir` ahora apuntan a
  `root:samples` en lugar de `root:samplelib`.
- **Gate de build estricto** (`scripts/build.sh`): falla si GCC emite cualquier
  `error:`/`warning:`/`note:` en formato real `file:line:col`, sin falsos
  positivos de nombres como `tinyxmlerror.cpp`.
- **Build 100% limpio**: `DIAGNOSTIC_GATE=0_ERRORS_0_WARNINGS`. Core
  `aa188e4eb1dcdc63a3d8b4c4d9532af581011d5669bd7cd627cb0e3825b5ed2d`.

## Release: Bacon 1.2.1 - Chopper UAF Hardening

- **Estado**: release de estabilidad sobre Bacon 1.2 (Mixer Dev). Corrige el
  crash del chopper (crop/delete/pitch/normalizar/undo/redo con reproducción
  activa) causado por un use-after-free del buffer de sample compartido.
- **Guard zombie de voces** (`SampleInstrument::Render`): si el buffer que una
  voz cacheó al dispararse ya no coincide con el buffer actual de la fuente, la
  voz termina en silencio en lugar de leer memoria liberada por
  `WavFile::ReplaceBuffer()`. Última línea de defensa del UAF.
- **Parada de audio completa en todas las ediciones destructivas del chopper**:
  Crop y Delete ya la aplicaban; ahora también Undo/Redo (L1+X/R1+X), Pitch/Env
  Apply (A) y Normalizar (R2+Y) detienen el patrón (`Player::Stop()`) y el
  stream de preview (`StopStreaming()`) ANTES de reemplazar el buffer.
- **Build 100% limpio**: corregido el warning `-Wreorder` preexistente del
  constructor de `SampleChopperModal` (orden de inicialización).
- **Diagnóstico**: instrumentación temporal (`boot_steps.log`,
  `chopper_debug.log`) confirmó que el SIG=11 f0 post-crash era un estado
  residual del launcher/emulador (boot del core íntegro) y que Crop/Pitch/
  Undo/Redo se ejecutaban con `running=1` durante las ediciones destructivas.
  La instrumentación fue retirada del release.

## Release: Bacon 1.2 - Mixer Dev

- **Estado**: release del desarrollo completo del mixer sobre la línea Bacon
  (consolida las iteraciones V6-V16 sobre Bacon 1.1.1): medidores estéreo L/R
  con medición pre-clip, undo/redo global real en todas las vistas,
  normalización de samples en el chopper y perfil USB de audio estéreo 48 kHz
  como default de instalación (`audio_usb_profile = STEREO_48K`).
- **Medidores estéreo L/R pre-clip (V6-V9)**: cada canal y el master dibujan
  dos barras independientes (L en x, R en x+2 con hueco), leyendo cada lado su
  propio pico post-pan; el clipeo se quita del medidor (el scan es PRE-clip,
  la zona roja +3 dB es alcanzable, el hardClip int16 se mantiene solo en el
  transporte). Fix real del par L/R (el buffer es interleaved: `c[0]` es
  siempre L y `c[1]` siempre R), buses de canal/stream sin clip
  (`SetClipBypass`), fader master moviendo el bus medido, escala de canal
  0..127 (127 = +2.1 dB) y barra grande del mixer corregida a 12 celdas.
- **Undo/redo global (V10-V16)**: `L1+X` deshace y `R1+X` rehace en Song,
  Chain, Phrase y Table con historiales de 16 entradas y snapshots capturados
  dentro de los mutadores reales (Song completo 2048 B, cursor preservado:
  undo/redo deshace la ÚLTIMA ACCIÓN, nunca la navegación). Reclamado
  opt-in (devuelven bool), de modo que el legacy A+B de clear/cut se
  conserva fuera de FieldView/MixerView. Solo/Mute del mixer deshacibles
  (mascaras ME_MUTE/ME_SOLO) y redo que restaura el valor post-edición.
- **A+B reset option (V10-V11)**: en vistas de edición, A+B restaura el
  parámetro enfocado a su default (el canal del mixer resetea a volumen 100,
  no 127, y pan 0).
- **Barras VU bottom-up (V14-V16)**: niveles de 3 px alineados al medidor Cue,
  banda roja 0 dB+ arriba, reset a 0 al detener playback, menús del mixer
  opacos (PostFlushDraw se salta con modal o páginas FX) y MIX page con
  `RET D:xxx R:xxx FX RETURNS` como cabecera y barras de 15 celdas.
- **Menú L1+A del mixer (V13)**: master → LIMITER (softclip Bypass/Subtle/
  Medium/Heavy/Insane), CLIP GAIN y saltos a las páginas DELAY/REVERB/EQ/COMP;
  track → FILTER/BITCRUSHER/PLAYBACK/FX SENDS/AUTOMATION aterrizando en la
  sección del Instrument view del canal.
- **Chopper (V9, V13-V16)**: `R2+Y` NORMALIZA el sample (ganancia a
  32767/peak 0 dBFS, undo/redo con L1+X/R1+X, boundaries intactos);
  `togglePitchMode` blindado con `hasWaveform_`; overlay de operación sin
  caja ASCII; el overlay de onda nunca se pinta sobre el Help.
- **Help (V13-V15)**: secciones CHOPPER/CHOP PITCH registradas, navegación
  L1/R1 siempre (aunque SELECT quede pulsado), guard anti-apilamiento y
  chopper visible bajo el help.
- **USB audio (U2523)**: el perfil por defecto de instalación pasa a
  `STEREO_48K` (`scripts/install.sh`, `install_stock.sh`, `verify.sh`,
  `device/otg_h37_apply_driver_mode.sh` + copia en SD), verificado por
  `VERIFY_U2523_OK` con `ERRORS=0`.
- **SD limpia**: eliminadas las carpetas corruptas `F:\lgpt\project` y
  `F:\lgpt\samplelib` (nombres con bytes basura, "Error irrecuperable" de
  chkdsk) y `F:\FOUND.000`; el launcher recrea el árbol en el primer arranque
  (verificado por `tests/test_copy_root_launcher.sh`).
- **Verificación**: build `BUILD_U2523_OK` (core
  `d3178aab497e...`, daemons SP404 `b75a2477226a...`, MIDI `3f0ea7a23db7...`,
  USB `53258f2b8b37...`), install + verify en SD (`VERIFY_U2523_OK`,
  `ERRORS=0`), suite de layout y launcher en verde.

## Release: Bacon 1.1 - Audio Driver Refact, Sampler Audio Added

- **Estado**: release de estabilización del driver de audio Sampler
  (SP404MKII) con refactor del módulo playback del daemon. Consolida RC9.7 +
  RC9.8 + U2.53.0 + U2.53.1. Los cuatro drivers de audio (Windows / Local /
  Android / Sampler) verificados funcionando correctamente en consola.
- **Pitido permanente del Sampler eliminado (U2.53.0)**: el rearmer desde
  cero del módulo playback (`r36s_sp404_host_audio_io.c`) sustituye el
  resampler polifase + feed EMA + feed-ratio + latency-trims por un
  passthrough puro ring→PCM: pop 2ch, gain 0.65, conversión a 4ch (L/R en
  ch1/2), silencio de período completo en starvation (nunca se re-emite audio
  stale) y drop-on-full del ring. El diagnóstico FIFO_DUMP confirmó que el
  contenido del fifo del core es audio limpio del proyecto; el tono full-scale
  provenía del path del daemon (re-emisión de buffers stale tras cambio de
  modo sin replug).
- **FIFO_DUMP (U2.53.0)**: captura los primeros 2 s del contenido crudo del
  fifo (pre-gain, 48 kHz estéreo) a
  `/mnt/sdcard/LGPT_OTG_LOGS/fifo_capture.wav` en cada stream primed, para
  distinguir si un tono lo genera el core (presente en el WAV) o el daemon
  (ausente).
- **Fix crash del daemon (U2.53.1)**: overflow del buffer FIFO_DUMP. El
  array de 32768 muestras (16384 frames estéreo) se indexaba como frames x2
  (hasta índice 65535), por lo que desde el frame 16384 (~341 ms de audio)
  el daemon escribía fuera de límites y moría con el primer golpe del
  proyecto, dejando el fifo sin lector (port congelado) y un WAV de solo
  cabecera. Fix: `FIFO_DUMP_BUF_FRAMES 32768` con buffer `[32768*2]` y
  guard/flush coherentes.
- **RC9.8**: fix del pitido constante del Sampler — feed EMA reseteado en
  cada open + rechazo de spikes transitorios + override de starvation (ratio
  1.0). El feed EMA sobrevivía los reconnects y quedaba clavado en el clamp
  1.08 tras un burst de ~1.85x, drenando el ring 8% más rápido.
- **RC9.7**: etiqueta `Sampler` sin sufijo `[OUT]`, barra de preescucha
  USB-REC sólida (`UIIntVarField`), descripción `SP404: console sound to
  sampler (EXT SOURCE)`.
- **Verificación**: build `BUILD_U2523_OK`, install + verify en SD
  (`VERIFY_U2523_OK`, `ERRORS=0`), retest en consola con los 4 drivers.

## Release candidate: RC9.7 - Audio driver Sampler (SP404MKII)

- **Estado**: iteración de acabado sobre RC9.6 centrada en el driver de
  audio Sampler (SP404MKII) y el menú USB-REC. No toca el motor FX/DSP, la
  persistencia ni el resto de modos de audio.
- **Etiqueta del driver**: el modal de selección de audio driver ya no
  muestra el sufijo `[OUT]`; la entrada pasa a llamarse solo `Sampler`
  (`AudioDriverModal.cpp`), con la descripción
  `SP404: console sound to sampler (EXT SOURCE)` procedente de
  `TreeFrogUac2Bridge_GetDriverModeDescriptionByIndex` (`TreeFrogUac2Bridge.cpp`,
  caso `U241_USB_OUT`).
- **Barra de preescucha USB-REC**: sustituidas las líneas `|` por una barra
  sólida dinámica de 24 celdas con la estética canónica del port
  (`UIIntVarField`: relleno CD_NORMAL invertido, resto CD_HILITE1 hueco),
  formato `IN [....]  %3d%%` (`UsbRecordModal.cpp`, DrawView).
- **Verificación**: build del target `BUILD_U2523_OK` (sha core
  `56851509...`, daemons SP404/MIDI idénticos a RC9.6), install y verify en
  SD (`VERIFY_U2523_OK`, `ERRORS=0`) con estado `USB_OUT` restaurado.

## Release candidate: Bacon 1.1 - FX Dev (RC5)

- **Estado**: iteración exclusivamente de layout/renderizado sobre RC4
  (P0-P8). Centra horizontal y verticalmente cada bloque tipo menú sobre la
  cuadrícula real de 40x30 y corrige el centrado global de modales. **No
  toca el motor FX/DSP, FxEngine, la persistencia, la navegación, los
  colores, el footer, el Chopper ni las cuadrículas del tracker.**
- **Geometría centralizada**: constantes de pantalla/banda en `UiDraw.h`
  (`kScreenWidth=40`, `kScreenHeight=30`, `kMenuBandTop=3`,
  `kMenuBandBottom=25`, `kFooterTop=27`, `kFooterBottom=29`). El cuerpo de
  un menú se centra en las filas 3..25, nunca en toda la pantalla.
- **API de layout ampliada**: `UiDraw::CenterTextX(text, viewportWidth)` y
  `UiDraw::MakeCenteredMenuLayout(rowCount[, labelWidth, valueWidth,
  spacing[, viewportWidth, contentTop, contentBottom]])` con margen máximo de
  1 celda (2 para separadores impares) y clamp correcto. Los layouts ya no
  asumen 40 columnas ni la banda 1..29.
- **Modales**: `ModalView::SetWindow` usa `(30-height)/2+topOffset_`,
  guarda `width_`/`height_` con getters y clampa el marco completo (bordes en
  `top-2..top+height+1`) a las filas 0..29. `TreeFrogProjectExitModal` centra
  su título e items dentro de las 28 columnas locales del modal y usa de
  verdad el `MenuLayout` (se elimina `(void)ml`).
- **Páginas MASTER del Mixer** (DELAY/REVERB/EQ/COMP): cada bloque centrado
  en banda 3..25 y centrado horizontal con columnas label/valor calculadas.
  `DrawBypassRow(labelX, valueX, y, ...)` usa la misma columna de valor que
  las filas de parámetros (la firma antigua delega en la nueva).
- **Mixer principal**: 9 barras VU (MST + 8 canales) de 1 sola celda de
  ancho, uniformemente espaciadas cada 4 columnas; `bankWidth=33`,
  `firstMeterX=3`. `drawVolumeBar`/`drawMasterBar` dibujan una sola columna
  (`totalCells=height`, sin loop `c<2`). Textos MST/00..07/valores centrados
  sobre el eje de cada barra; FX RETURNS ocupa su propia fila (antes podía
  colisionar con un mute).
- **Tests**: nuevo modelo geométrico `tests/test_ui_centered_layout.py`
  (12 comprobaciones de bounding box, paridad de anchos/altos, posiciones
  de los 9 metros y enums FX intactos); actualizados los tests RC3/RC4
  para comprobar semántica y layout en vez de coordenadas mágicas. Suite
  completa en verde (`AUDIT_CLEAN_MAIN_U2523_OK`) y build del target
  (`BUILD_U2523_OK`, `SHA256SUMS.txt`).

## Release candidate: Bacon 1.1 - FX Dev (RC3 full)

- **Estado**: fase completa de la modernización visual integral
  (`docs/PLAN_RC3_MODERNIZACION_VISUAL_ES.md`, sección C). No toca el motor
  FX/DSP/persistencia (compatibilidad bit-idéntica intacta).
- **MixerView MASTER**: título de página centrado con
  `UiDraw::DrawCenteredTitleAt` (fila 1), bypass de DELAY/REVERB con
  `UiDraw::DrawToggle`; las leyendas permanentes de las filas 22/23 y de la
  página MIX se retiraron de la pantalla y migraron a la ayuda contextual
  (`HelpRegistry`, sección MIXER, abierta con SELECT+R1).
- **InstrumentView**: cabeceras de bloque (INSTRUMENT/FILTER/BITCRUSHER/
  PLAYBACK/EFFECT SENDS/AUTOMATION) renderizadas con
  `UiDraw::DrawSectionHeader`; el hint `R1+RIGHT USB REC` migró a Help.
- **Graphical Chopper** (punto 18): confirmado gráfico vía overlay
  framebuffer (forma de onda real, región seleccionada, marcadores de
  corte, eje y cursor). Los tres marcos ASCII restantes (frame, overlay de
  progreso, panel Pitch/Env) quedan documentados como excepciones en
  `OBSOLETE_FEATURE_AUDIT`/`UI_VISUAL_AUDIT`.
- **Widgets ASCII** (punto 17): sin widgets ASCII no documentados en la
  capa de vistas; `--`/`----` son placeholders de valor legítimos.
- **Pruebas**: nuevo `test_rc3_full_views_modernization.py`; auditoría
  completa en verde (`AUDIT_CLEAN_MAIN_U2523_OK`); core reconstruido,
  instalado y verificado en SD (`BUILD_U2523_OK`, `INSTALL_U2523_OK`,
  `VERIFY_U2523_OK`, `ERRORS=0`).

## Release candidate: Bacon 1.1 - FX Dev (RC3 base)

- **Estado**: fase base de la modernización visual integral
  (`docs/PLAN_RC3_MODERNIZACION_VISUAL_ES.md`). No toca el motor FX.
- **Bypass unificado**: en las páginas master DELAY/REVERB/EQ/COMP el
  `BYPASS` es ahora la primera fila visual y lógica (semántica
  `ON = efecto desactivado`). Helpers de fila ordenada (`fxBypassId`,
  `fxIdForRow`, `fxCountOnPage`) compartidos por navegación y dibujo; la
  tabla `kFxParams_` y el enum quedan byte-idénticos (persistencia
  bit-idéntica).
- **Librería UiDraw**: primitivas compartidas `DrawCenteredTitle`
  (título centrado `x=(40-len)/2`), `DrawSectionHeader`, `DrawValueRow`,
  `DrawToggle` (`[ ON ]`/`[ OFF ]`), `DrawSolidBar`, `DrawBipolarBar`,
  `DrawProgressBar`, `DrawTabs`, `DrawModalFrame`, `DrawScrollIndicator`
  y `DrawSeparator`. Todo dibujo se clampa a 40x30.
- **Colores semánticos** (`UiColors.h`): roles `UI_COLOR_*` mapeados a los
  `CD_*` existentes; sin RGB directo por vista.
- **Help centralizado**: `HelpRegistry` (sección por vista) + `HelpOverlay`
  (latch, no propaga input). `SELECT+R1` abre la ayuda contextual desde
  cualquier pantalla; `SELECT+R2` conserva el diálogo de Audio Driver.
- **Auditorías**: `UI_CONTROL_AUDIT.md`, `OBSOLETE_FEATURE_AUDIT.md`,
  `UI_VISUAL_AUDIT.md` y la guía `UI_STYLE_GUIDE.md`.
- **Pruebas**: nuevo `test_rc3_base_unified_bypass_uidraw_help.py`; suite
  completa en verde (`AUDIT_CLEAN_MAIN_U2523_OK`). Core instalado y
  verificado en SD (`VERIFY_U2523_OK`, `ERRORS=0`).

## Release candidate: Bacon 1.1 - FX Dev (RC2)

- **Estado**: release candidate sobre RC1. Añade la **normalización de
  etiquetas FX visibles (tabla T1)** sin tocar los FourCC internos (los
  proyectos siguen siendo bit-identicos): `FBMX->CFM`, `FBTN->CFT`,
  `DLAY->NDL`, `FLTR->FCR`, `FCUT->FCU`, `FRES->FRS`, `CRSH->BCR`,
  `PFIN->PFT`, `DLYS->DSE`, `RVBS->RSE`, `DLYT->DTM`, `DLYF->DFB`,
  `RVDC->RDC`, `RVSZ->RSZ`, `CMPT->CTH`.
- **Selector FX por familias con paginación**: FX 1/2 (INST/FILTER/DELAY/
  REVERB/MASTER) y FX 2/2 (LEGACY COMB, CFM/CFT); los proyectos con
  FBMX/FBTN siguen reproduciéndose y siendo editables.
- **Reverb wet-only**: `RVB MIX` retirado de la UI (inerte en el DSP),
  headroom -3 dB de entrada, suma de combs normalizada (`combNorm_`) y 3er
  allpass por canal en NORMAL. El Delay conserva su dry/wet.
- **Páginas DELAY/REVERB MASTER dedicadas**: menús de dos columnas con
  jerarquía de colores (título `CD_HILITE1`, label `CD_NORMAL`, valor
  `CD_HILITE1`, fila editada invertida `CD_HILITE2`) y unidades
  (`ms`, `s`, `ON/OFF`, `ECO`/`NORMAL`).
- **Barras sólidas en InstrumentView**: EFFECT SENDS con bloque sólido de
  celdas invertidas (estilo MixerView) en vez de `[====----]`; `INH` limpia
  la barra.
- **`UIBigHexVarField::SetHexMode` corregido**: conserva el cursor de nibble
  (clamp al rango) y clampa/envuelve el valor al nuevo `[min,max]`.
- **Pruebas**: suite completa en verde (`AUDIT_CLEAN_MAIN_U2523_OK`).
  Nuevo `test_fx_rc2_master_pages_solid_bars_hexmode.py`; actualizados
  `test_fx_phase10_wetonly_audit`, `test_fx_phase4_ui`,
  `test_fx_phase6_nav_ab_default`, `test_fx_phase12_eq_menu`,
  `test_fx_phase13_comp_menu` y `test_fx_phase8_instrument_blocks`.
- **Binario**: `BUILD/U2523/lgpt_r36sx_u2523.so`
  (SHA-256 `c114863b8c43d6ae1300dd672edc5b6980970f59ec08bc29ceb658b58126bc20`),
  daemon ABI7 golden inalterado. Detalles en `docs/RELEASE_BACON_1.1_FX_DEV_ES.md`.

## Release candidate: Bacon 1.1 - FX Dev (RC1)

- **Estado**: release candidate (no estable) sobre la línea Bacon. Integra el
  rediseño FX completo (Fases 0-18 de `docs/PLAN_FX_REDESIGN_ES.md`):
  sends por instrumento DRY/DLY/RVB (live por canal, defaults `100/0/0`),
  automatización live `DLYS`/`RVBS`, retornos globales `DLYRET`/`RVBRET`,
  páginas dedicadas Delay/Reverb/EQ/Comp y branding `LGPT R36SX - Bacon 1.1`.
- **Pruebas**: suite completa en verde (`AUDIT_CLEAN_MAIN_U2523_OK`, 176
  checks OK, 27/27 grupos de audit). Tests preexistentes corregidos:
  `test_u2520_transactional_record_source` (ruta temp `/tmp/r36sx_lgpt_record`),
  `test_u2521_browser_rename`, `test_u2522_nested_rename_frame_forwarding` y
  `test_u2523_rename_caret_alignment` (verifican la implementación actual con
  `TreeFrogTextEditor`; el modal `ImportBrowserRenameModal` fue eliminado en H38.x).
- **Binario**: `BUILD/U2523/lgpt_r36sx_u2523.so`
  (SHA-256 `fddc4b042742da0745edf4f24edeee66543e67b900ecb03b87196bc41f8764ec`),
  daemon ABI7 golden inalterado. Detalles en `docs/RELEASE_BACON_1.1_FX_DEV_ES.md`.

## FX redesign Fase 14 (general fine/coarse curve editing)

- The Fase 12 musical curve edit is generalized: `fxUsesCurve()` + `fxEditCurve()` apply semitone fine (L/R) / octave coarse (A+UP/DOWN) proportional steps to every wide-range time/ratio param — EQ frequencies plus `DLY TIM`, `RVB PRE`, `RVB DEC`, `CMP ATK`, `CMP REL`, `CMP RAT`. Relative error is constant, so the full range is reachable in a bounded number of presses (e.g. CMP REL 1..2000 ms in ~132 fine / ~11 coarse).
- Below-floor values never get stuck: values below vmin snap to the floor on the first upward edit (e.g. DLY TIM default 0, vmin 10); when the floor is 0 (RVB PRE) the first edit starts at 1% of the range. Clamped to [vmin, vmax] both ways. Other (linear) params keep fine=1 / coarse=10.
- New test `test_fx_phase14_curve_editing.py` -> `FX_EDIT_CURVE_PHASE14_OK`; MixerView compiles, full FX suite green, `HOST_SYNTAX_CHECK_U2523_OK`.

## FX redesign Fase 13 (dedicated COMP menu with BYP first and visible GR)

- The COMP page is now a dedicated, exclusive menu (`drawCompPage()`); CMP BYP is the first row so it is never off-screen. Centered rows with a fixed value column: `Bypass`, `Threshold -24.0 dB`, `Ratio 4.0:1` (rendered as x:1), `Knee 6.0 dB`, `Attack 15.0 ms`, `Release 200.0 ms`, `Makeup 0.0 dB`, `Stereo Link ON`, `Soft Clip ON` (booleans as ON/OFF, units shown).
- `Gain Reduction -00.0 dB` GR meter stays visible below the parameters (readout, not selectable); navigation hints remain outside the parameter area (rows 22-23); no row overlap (params 2..10, GR at 12).
- No clipping indicator: the engine has no reliable real audio clip reading (`GetRtViolations` is buffer RT telemetry that must stay 0), so none is added. Soft clip is labelled "Soft Clip" only (never "limiter").
- The COMP param enum was reordered to BYP first (then THR/RAT/KNE/ATK/REL/MKU/LNK/SC), same IDs 28..36; Fase 4/6 test models updated.
- New test `test_fx_phase13_comp_menu.py` -> `FX_COMP_MENU_PHASE13_OK`; `MixerView.cpp` compiles, full FX suite green, `HOST_SYNTAX_CHECK_U2523_OK`.

## FX redesign Fase 12 (dedicated EQ menu with banded layout)

- The EQ page is now a dedicated, exclusive menu (no longer the generic parameter list): `drawEqPage()`/`drawEqRow()` render `EQ BYPASS [ ON ]` on top and LOW/MID/HIGH band blocks (header + EN ON/OFF, FRQ in Hz, GAIN with signed dB, Q). Every parameter is its own selectable row, so selection is unambiguous and the whole band stays visible while editing.
- The EQ param enum was reordered so each band is EN first, then FRQ/GAI/Q, matching the visual order UP/DOWN walks; Fase 4/6 test models updated (IDs 15..27 and per-page counts unchanged).
- Frequency editing is musical: fine (L/R) steps one semitone, coarse (A+UP/DOWN) one octave, clamped to 20..20000 Hz; the full range is reachable in ~120 fine or ~10 coarse presses.
- New test `test_fx_phase12_eq_menu.py` -> `FX_EQ_MENU_PHASE12_OK`; `MixerView.cpp` compiles (`MIXERVIEW_SYNTAX_OK`), full FX suite green, `HOST_SYNTAX_CHECK_U2523_OK`.

## FX redesign Fase 9-11 (MIX FX RETURNS + wet-only audit + [n/5])

- Fase 9: the MIX page per-track D/R send readouts are gone (sends are per-instrument since Fase 6/7 and are edited in InstrumentView; the per-track sends remain only as the Fase 7 inheritance layer). The MIX page now shows an editable FX RETURNS readout for the master delay/reverb return levels (`RET D:50% R:50%`), edited by cycling the R2 target VOL -> DLY RET -> RVB RET. Returns are global master levels, persisted as `DLYRET`/`RVBRET`.
- Fase 10: audit confirmed the global SEND/RET rows are gone from the FX pages and that DLY MIX / RVB MIX default to 1.0 (full wet) so the default return is wet-only (no dry leak); lowering MIX is the documented dry/wet crossfade, not a regression.
- Fase 11: every FX page title shows its position `[n/5]` (`DELAY MASTER [2/5]`, etc.); the MIX page hint shows `SELECT [1/5]`.
- New tests: `test_fx_phase9_mixer_returns.py` -> `FX_MIXER_RETURNS_PHASE9_OK`, `test_fx_phase10_wetonly_audit.py` -> `FX_WETONLY_AUDIT_PHASE10_OK`; `test_fx_phase4_ui.py` updated -> `FX_UI_PHASE43_OK`.

## FX redesign Fase 7-8 (track-send compat + InstrumentView blocks)

- Fase 7: controlled non-destructive per-track send compatibility. An instrument without an override (-1) inherits the per-track Mixer send; an instrument override wins per-instrument. Both layers are persisted (Mixer CHANNEL `DELAYSEND`/`REVERBSEND` + instrument PARAMs `DRY`/`DLY_SEND`/`RVB_SEND`); the per-track send is never deleted.
- Fase 8: the sample InstrumentView is reorganized into vertical blocks (INSTRUMENT / FILTER / BITCRUSHER / PLAYBACK / EFFECT SENDS / AUTOMATION) with two-column field rows. Block headers are drawn in DrawView (not as UIStaticField) so L2+A cut/clear still see the sample field first and the table field last.
- New `UIIntVarField::SetBar(label, width)`: percent-bar rendering for the EFFECT SENDS rows (`DRY`/`DELAY`/`REVERB`, with `INH` when a send inherits the per-track value). Existing field rendering is unchanged by default.
- BITCRUSHER block is labeled "bit depth" (never "compressor"); fb tune/fb mix are retired from editing but their variables stay for load/playback; print fx/wet/pad moved behind `#ifdef FFMPEG_ENABLED` (absent from the R36SX build).
- The whole sample-instrument layout now fits above the bottom map/notes band (max row 26; map/notes at y=27-29).
- New test `tests/test_fx_phase8_instrument_blocks.py` -> `FX_INSTRUMENT_BLOCKS_PHASE8_OK`.

## FX redesign Fase 6 (per-instrument sends + 5-page mixer + A+B=default)

- Per-instrument FX sends: each sample instrument has DRY, DLY send and RVB send variables. DRY scales the effective send (`gain = send*DRY/10000`); the instrument override wins over the per-track Mixer send, and DRY=100 is bit-identical to the previous behaviour.
- DLYS/RVBS phrase commands now write both the instrument override and the per-track Mixer send.
- Mixer FX pages redesigned: MIX / DELAY / REVERB / EQ / COMP (5 pages, 37 parameters). The global SEND/RET rows were removed (sends are per-track/per-instrument, returns stay fixed).
- The Instrument FX modal is gone: R2+A in the Mixer jumps straight to the Instrument view for the hovered channel.
- New FX navigation: A+B restores the focused parameter to its default, on both the Mixer FX pages and the Instrument view fields.
- The legacy "B+A cut instrument / clear table" action moved to L2+A (A+B now means "restore default").
- The Instrument view shows a live `fx sends: dry/dly/rvb` readout.

## H38.6

- New dedicated Pitch column in the phrase grid (`N V P I FX1 P1 FX2 P2`): each step can be transposed -24..+24 semitones, edited with L/R (+-1) and A+UP/DOWN (+-10). Persisted in the project (new `PITCHES` buffer) and applied per note at playback. Chop rows (S01..S99) are protected so the pitch never selects a different chop.
- PTCH command removed from the FX command list (old projects with PTCH in FX are ignored on playback; the pitch is now a per-step column).
- Phrase volume fix: every value 0-100 now maps linearly (100 = full, 1 = ~2%, 0/0xFF = silent), no clipping or distortion at any level; new notes default to volume 100.
- Mixer VU meters now refresh in real time even while the player is stopped (same frame cadence as the USB-C Record meter).
- Rename Project moved from the in-project menu (crash when re-entering TreeFrogUI) to the startup menu: R1+A on a selected project.
- Unified text editor input everywhere (USB-C Record, project rename, new project, sample rename): X+UP/DOWN fast, L1+X case, A confirms, B erases, R1+LEFT cancels.
- New project and Save As use the same Record-style text editor (QWERTY keyboard removed).

## H38.5

- Removed FX3 from phrase editing and playback (Phrase: `00 N V I FX1 P FX2 P`; Table: `F1 P1 F2 P2`).
- Centered phrase and table grids with per-column headers.
- Volume and FX intensity scale now treat 1 as 100% (higher values attenuate).
- Dense mixer VU meters in Record style.
- Rename Project action in the Project menu.

## U2.52.3

- Stable bidirectional USB-C OTG audio at 48 kHz.
- Rewritten Record workflow with transactional Preview, Save and Discard.
- Input monitor restricted to Record.
- Chord-aware input handling and Chopper Undo/Redo.
- Safe sample rename and deferred sample deletion.
- Fixed nested rename input forwarding and caret alignment.
- Consolidated source, scripts, tests and bilingual documentation.

Earlier experimental iterations were consolidated into this release and are available through Git history, not as duplicate files in the current tree.
