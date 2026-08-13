# LGPT R36SX — Bacon 1.2 — Mixer Dev

## Actualización U2.69: fix de tirones en Sampler (SP404MKII)

El daemon `r36s_sp404_host_audio_io` ya no produce los cortes de 10 ms
(~2/s) que se oían en el Sampler: el ASRC corrige ahora la deriva real del
reloj del core (~0,45 %) aplicando siempre el lazo P/I de backlog en vez del
passthrough fijo 1:1. Limpia también el flush de logs a la SD (`cp -f`).

# LGPT R36SX — Bacon 1.2 — Mixer Dev

Release de desarrollo del mixer del port LGPT R36SX (consolida las
iteraciones V6-V16 de Bacon 1.1.1).

## Qué hay de nuevo en Bacon 1.2

### Medidores estéreo L/R pre-clip (V6-V9)

- Cada canal y el master dibujan DOS barras independientes (L en x, R en x+2
  con hueco): el paneo se ve en las barras (centro = iguales, L duro =
  izquierda llena / derecha vacía).
- El clipeo se quita del MEDIDOR: `AudioMixer::Render` aplica el damp del
  master antes de medir, escanea el pico L/R PRE-clip (la suma puede exceder
  1.0 / 0 dB y la zona roja +3 es alcanzable) y después satura para int16.
- Fix real del par L/R: el buffer es interleaved (`c[0]` es SIEMPRE L y
  `c[1]` SIEMPRE R); el scan muestrea el par por cada 8 samples en
  `AudioMixer` (pre-clip) y `PlayerChannel` (post-pan).
- Buses de canal/stream sin clip (`SetClipBypass`): el master mide la suma
  real pre-clip del bus. `SetMasterVolume` alimenta `master_` (bus medido)
  en vez de `out_`: el fader mueve la barra del master.
- Escala de volumen de canal 0..127 (127 = +2.1 dB, el relleno puede pasar
  0 dB y llegar a la zona roja; `fp2i` hace wrap sobre 1.0 y el hardClip
  int16 se mantiene).
- Fix de la barra grande del mixer (`PostFlushDraw` pintaba 12 PIXELES en
  vez de 12 CELDAS): `yBase=(py+row*8)*ancho`.

### Undo/redo global (V10-V16)

- `L1+X` deshace y `R1+X` rehace en Song, Chain, Phrase y Table con
  historiales de 16 entradas; los snapshots se capturan DENTRO de los
  mutadores reales (Song completo 2048 B, guard de reentrada para
  selecciones multi-fila), con guard anti-apilamiento y redo conservado
  hasta la siguiente edición.
- Undo/redo deshace la ÚLTIMA ACCIÓN, nunca la navegación: los snapshots de
  Song/Phrase ya NO restauran el cursor; en FieldView la navegación entre
  campos nunca empujó historia.
- `L1+X`/`R1+X` fiables con máscaras latcheadas: `AppWindow` resuelve el
  acorde en `ET_PADMASKDOWN` (gana el hombro recién pulsado; si ninguno es
  fresco, el último hombro pulsado; último recurso, la dirección del último
  acorde) y repite la resolución en `ET_PADBUTTONDOWN`; un hombro suelto se
  retiene un poll esperando a que se le una X.
- Solo/Mute del mixer deshacibles: `ME_MUTE` (estado 0/1) y `ME_SOLO`
  (máscara de mute de 8 canales antes/después); el undo de solo restaura la
  máscara vía `Player::SetChannelMute` sin tocar `UIController::soloMask_`.
- Redo correcto: `MixEdit.newValue` captura el valor post-edición en el push
  (vol/pan/master/returns/fx/softclip/clipgain) y `FieldView` hace lo mismo.

### A+B reset option

- En vistas de edición, A+B restaura el parámetro enfocado a su default.
- El canal del mixer resetea a volumen 100 (no 127) y pan 0; master ya era
  100. Los combos globales se reclaman opt-in (devuelven bool), de modo que
  el legacy A+B de clear/cut en Song/Phrase/Table/Chain se conserva.

### Barras VU bottom-up (V14-V16)

- Niveles de 3 px (2 px de relleno + 1 px de hueco): 32 pasos finos por
  barra de 12 celdas; zona 0 dB+ = 12.5% superior rellena en rojo sólido
  solo cuando el nivel llega.
- Barras rellenas de abajo hacia arriba alineadas con el medidor Cue; banda
  roja en la parte superior desde 0 dB (nivel 36/39 de la escala -36..+3 de
  `mixVULevel`).
- Al detener el playback los picos se muestrean como 0 (decaimiento real).
- `PostFlushDraw` se salta con un modal abierto o páginas FX: los menús del
  mixer ya no se entrecortan con las barras.
- MIX page: `RET D:xxx R:xxx FX RETURNS` sube a la fila superior segura (3)
  como cabecera y las barras crecen a 15 celdas (filas 5..19, números en 21).

### Menú L1+A del mixer (V13)

- Master: LIMITER (softclip Bypass/Subtle/Medium/Heavy/Insane con L/R +
  undo ME_SOFTCLIP), CLIP GAIN (`[unity]`/`[boost]`) y saltos A a las páginas
  DELAY/REVERB/EQ/COMP (`JumpToFxPage`).
- Track: FILTER/BITCRUSHER/PLAYBACK/FX SENDS/AUTOMATION que abren el
  Instrument view del canal aterrizando en la sección
  (`instrumentFocusHint_` en `ViewData`).

### Chopper

- `R2+Y` NORMALIZA el sample: scan de pico, ganancia a 32767 (0 dBFS),
  `ReplaceBuffer` + `SaveBufferToPath`, undo/redo con L1+X/R1+X, boundaries
  intactos por tamaño fijo, status `peak -> 32767`. Libre en todos los modos
  (L2+Y sigue siendo delete).
- `togglePitchMode` blindado con `hasWaveform_` (anti-crash con L1+R1 sin
  forma de onda cargada).
- Overlay de operación sin caja ASCII (título centrado + filas label/value
  estilo Pitch/Env); el overlay de onda nunca se pinta sobre el Help
  (`suspended_` mantiene `g_chopperOverlayActive=0` bajo un modal empujado).

### Help

- Secciones CHOPPER + CHOP PITCH registradas; `SELECT+R1` dentro del chopper
  abre el help con la sección del chopper; L1/R1 navegan secciones en modo
  contenido (L2/R2 primero/último), tab actual resaltado invertido.
- Con un Help abierto, L1/R1/L2/R2 se reenvían SIEMPRE al overlay (solo los
  bits de navegación, sin SELECT); el cierre es B.
- Guard anti-apilamiento: `SELECT+R1` con Help ya abierto no apila un segundo
  Help (`IsHelpOverlay()` virtual en `ModalView`).
- `View::Redraw` dibuja el modal suspendido (el chopper) bajo el overlay
  empujado.

### USB audio y SD

- Perfil USB por defecto `STEREO_48K` en instalación (`install.sh`,
  `install_stock.sh`, `verify.sh`, `otg_h37_apply_driver_mode.sh`),
  verificado por `VERIFY_U2523_OK` con `ERRORS=0`.
- SD limpia: eliminadas `F:\lgpt\project` y `F:\lgpt\samplelib` (carpetas
  corruptas que chkdsk convirtió en archivos) y `F:\FOUND.000`; el launcher
  recrea el árbol de carpetas en el primer arranque.
- Build 100% limpio (0 warnings; Makefile.TREEFROG silencia categorías
  legacy del árbol upstream y mantiene `-Wmaybe-uninitialized`).

## Verificación

- Build: `BUILD_U2523_OK` (core `d3178aab497e...`, daemons USB
  `53258f2b8b37...`, SP404 `b75a2477226a...`, MIDI `3f0ea7a23db7...`).
- Install + verify en SD: `INSTALL_U2523_OK`, `VERIFY_U2523_OK`, `ERRORS=0`.
- Suite de layout y launcher: `VERIFY_LAYOUT_OK`,
  `TEST_LAUNCHER_AUTOCREATES_SAMPLELIB_OK`.

## Contenido del ZIP

- `cubegm/`, `lgpt/`, `roms/`, `LGPT_OTG_LOGS/` y `ANDROID/`: payload listo
  para copiar a la raíz de la SD (copia de raíz completa, incluye las APKs
  del puente de audio Android `LGPTUsbAudioBridge-H36-debug.apk` y
  `-H38-debug.apk`).
- `SOURCE_AND_TOOLS/full_repository`: snapshot completo del repositorio
  (fuente, scripts, tests y documentación).
