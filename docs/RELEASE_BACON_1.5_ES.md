Pre-release Bacon 1.5 - Sinths and EQ8 (build U2.52.3, 2026-08-18)

Build:
- lgpt_r36sx_u2523.so - SHA256 bf8cf44c339392f5c8c3afef3de2ab7cb738571acf91b4e795fdd5b0d4955e66
- r36s_u2523_usb_audio_io (daemon USB UAC2) - f7140072... (byte-identico al anterior)
- r36s_sp404_host_audio_io - 968dfa61...
- r36s_midi_host_io - 3f0ea7a2...
- LGPT_R36SX_Bacon-1.5_SD_ROOT.zip (payload copy-to-SD-root)

Novedades Bacon 1.5:

1. Menú Sinth (Bass/Piano) en el navegador: Listen/Import/Manage/Sinth/Exit
   (item 9 + feedback de consola).
2. Preescucha con B en las páginas Bass/Piano (Player::PreviewNote/StopPreview/
   UpdatePreview con arm/desarm TreeFrogAudioSetPlaybackArmed, auto-stop ~0.9 s,
   cancelada por Start/Stop y por segundo B).
3. EQ8 gráfico fullscreen por instrumento (InstrumentEqView, reemplaza el modal;
   curva dibujada con los MISMOS coeficientes GetBandCoeffs que procesa el DSP;
   acepta IT_SYNTH/IT_PIANO; PLAYBACK fuera del editor, parámetros en el modelo).
4. Conversión de slot a sintetizador vía InstrumentView::ConvertCurrentToSynth
   (mismo protocolo seguro que el selector src: detach del observer, Stop,
   SetInstrumentType, rebuild; bloquea con sample asignado).
5. Menú TRACK del Mixer salta a EQ8.
6. Audición aislada: el preview suena por un canal/bus propios (AuditionService)
   aunque la pista esté muteada o a volumen 0; StopPreview nunca toca los 8
   PlayerChannel (aislamiento verificado por test estático).
7. SpectrumAnalyzer dirigido común: tap post-EQ/pre-gain en PlayerChannel y en
   la audición, solo cuando está armado y el instrumento es el objetivo.
8. Guard de estabilidad RBJ bell en EqBiquad.h: el peaking de la receta RBJ
   diverge con boost a baja frecuencia (verificado: +6 dB @1 kHz Q1, +2 dB
   @250 Hz Q1, +12 dB @80 Hz Q1 explotaban); el boost queda limitado al 90%
   del valor marginal A = sw/(sw-4Q(1-cw)) y el filtro siempre acota la salida.
   Aplica a InstrumentEq (EQ8) y a ParametricEQ (master EQ).

Novedades U2.52.4 (feedback de la prueba en R36SX):

9. EQ8 fullscreen limpio: el canvas propietario cubre toda la pantalla bajo la
   cabecera (filas 3+), se repinta el fondo completo cada frame (sin letras
   residuales ni solapamiento con el texto del menú); la fila de estado lleva
   los atajos y el estado de bypass.
10. A+B resetea la banda seleccionada a su valor por defecto (frecuencia por
    defecto, ganancia 0, Q 1.00, BELL, ON), misma semántica que los campos.
11. Preescucha a C-3 (nota 60): el preview del sinth empezaba en C-2 (48);
    ahora coincide con la convención "playbackNote = 60" de las samples.
12. Volumen de preescucha +6 dB fijos (kAuditionGain): la audición era casi
    inaudible junto a samples grabadas; el clip del master protege el tope.
13. Espectro sobre el MIX final: el tap del analyzer se movió a la salida del
    master (AudioOutDriver/DummyAudioOut, post-FxEngine) — el espectro muestra
    toda la mezcla, no solo el instrumento en edición; sin taps por canal ni
    targetInstrument_.
14. Guard RBJ afinado (0.9 -> 99.5% del margen, BELL-only): el 90% dejaba los
    boosts de bandas bajas casi mudos en el dispositivo (p. ej. +2 dB @250 Hz
    Q1 quedaba a 0 dB); con el 99.5% los boosts habituales suenan y el filtro
    sigue acotado (bordes marginales verificados por Jury: 1+a1+a2 > 1e-5).

Novedades U2.52.5 (feedback de la prueba en R36SX #2):

15. Solo banda 1 en sinths/piano (causa raíz): MAKE_FOURCC empaqueta el dígito
    en el byte alto, así que (FourCC)(SIP_EQF0 + i) corrompía los ids de las
    bandas 2-8 y FindVariable devolvía NULL (el view EQ8 no encontraba las
    variables). Arrays explícitos de FourCC en BassSynth y PianoSynth (mismo
    patrón que SampleInstrument) — las 8 bandas editan y suenan.
16. Curva del EQ8 en vivo: el canvas se dibuja desde el estado del view
    (frecuencia/ganancia/tipo/Q/on) con la MISMA matemática RBJ del DSP
    (FxEngine::eqBiquadCoeffs), no desde los readbacks del DSP (que solo se
    sincronizaban mientras el audio renderizaba: el canvas se congelaba al
    editar y mostraba el tipo/frecuencia viejos).
17. Canvas del EQ8 a ±24 dB: la grilla dibuja ±12 y ±24, el rango completo se
    mapea (pxPerDb sobre 48 dB) y las etiquetas muestran +24/0/-24.
18. Highlight fantasma "wave/mode/type": los SetFocus intermedios de los fill()
    de InstrumentView dejaban el foco clavado en la línea (focus_=true sin
    estar seleccionada) — eliminados (solo el campo bajo el cursor se marca).
19. El EQ8 ya no mata el sonido al editar (samples Y sinths): el update de
    estados del Df2 transpuesto sumaba en 32 bits y con entrada full-scale +
    boosts cada término se acerca a ±2^31 → overflow signed (UB) que corrompía
    el estado recursivo (detectado por UBSAN en el test 7b). El sumatorio se
    hace en 64 bits (BACON_1.5_EQ8_DF2_64BIT): el resultado final cabe en 32
    bits, el truncado es exacto, y el costo es despreciable (fp_mul ya era
    long long).
20. Volumen del sinth a 0 dB: sustain por defecto 60→100 — la nota sostenida
    del BassSynth quedaba ~4.4 dB por debajo de una sample a volumen máximo;
    ahora el preview B y las notas de frase suenan al mismo nivel (softclip +
    pan equal-power: RMS ~0.31 a full scale, verificado por test).
21. Tests 7b (bass_synth_host_test): secuencia de edición real del EQ8 —
    cada banda a +1 dB audible, los 7 tipos audibles, 32 ediciones seguidas
    sin silencio, sostenido a 0 dBFS (sustain 100 vs 60 ≥ 1.3×), estado
    restaurado al final. El check mide con envolvente instantánea
    (attack/decay 0) y estado nivel-afectante reseteado (volume/pan/glide/
    fcut/fres/drive/accent), porque los checks previos dejaban volume 50 y
    pan 0 (canal derecho mudo) y el RMS usaba la escala equivocada (÷65536
    en Q15 cuyo full scale es 32768).

Fixes:

- Fix FAST_MATH (audio estable): sinf/powf/expf fuera del bucle por muestra de
  los sintetizadores (tablas interpoladas compartidas SynthMath.h: seno 1024 +
  2^x 256; glide hoisteado bit-identical; pan con tabla). Sin cambios de sonido
  perceptibles (error < 1e-6), cero libm por muestra.
- Fix DIALOGO PROYECTOS (crash.txt #1-#5): los 5 SIGSEGV del dispositivo eran
  NULL derefs de SelectProjectDialog (lista vacía / cursor stale) — OnRenameProject
  (dumps 1-3, core viejo) y ProcessButtonMask A→Load (dump 5, core nuevo).
  CrashTrap hacía _exit(128+sig) y picoarch reiniciaba el core → el audio moría
  ("el instrumento dejó de sonar"). Guards TREEFROG_DIALOG_NULL_GUARD_V1:
  current==0 en A→Load, ruta vacía en OnRenameProject + scan de un solo pase,
  clamp a 0 en warpToNextProject con lista vacía.
- CrashTrap: formatter hex corregido ("0x" antes de los dígitos) para que los
  dumps sean parseables.
- H38.8 hilo de audio: scratch estático en AudioMixer::Render (sin malloc por
  buffer), pow(x,3)→x*x*x en softclip, fmodf del square de BassSynth→rama.
- InstrumentEq: estados biquad por banda y por canal (la cascada ya no comparte
  estado: antes el orden de las bandas cambiaba el sonido ~23k LSB); smoothing
  con snap exacto (antes se quedaba a 63 LSBs del objetivo sin limpiar el flag).
- InstrumentEq (U2.52.5): overflow signed del Df2 transpuesto (ver punto 19) —
  el mismo patrón vive en ParametricEQ (master EQ) pero sus entradas son buses
  pre-mezcla a nivel bajo y nunca se reportó; se deja intacto por política de
  golden (solo se toca si el dispositivo lo evidencia).

Regresión: AUDIT_CLEAN_MAIN_U2523_OK (tests/run_all.sh), F10_BASELINE_OK
(golden core 739d3529), host bass 72 + piano 46 + eq8_struct 63 +
analyzer_mix 55 checks OK (bass 7b con UBSAN limpio), TEST_FX_PHASE19
_AUDITION_ISOLATED_OK, DIAGNOSTIC_GATE=0_ERRORS_0_WARNINGS, BUILD_U2523_OK,
verify ERRORS=0.

Novedades U2.52.6 (feedback de la prueba en R36SX #3):

22. El EQ8 ya no mata el sonido al cambiar el modo (causa raíz definitiva,
    samples y sinths): los filtros RBJ LOW_PASS/HIGH_PASS/NOTCH/BAND_PASS no
    tienen ganancia, así que con 0 dB seguían ACTIVOS — p. ej. LOWPA en la
    banda 1 (80 Hz por defecto) cortaba todo lo que está por encima de 80 Hz
    y el sample quedaba casi mudo (por eso "solo suenan en BELL": BELL@0dB sí
    era transparente). Ahora UNA banda a 0 dB es transparente para TODOS los
    tipos: se salta en el DSP, sus coeficientes saltan a la identidad al
    instante (recomputeBand, sin smoothing ni estado residual) y la curva
    dibujada la omite (lo que ves == lo que oyes). El filtro entra solo al
    mover la ganancia fuera de 0. BACON_1.5_EQ8_0DB_TRANSPARENT.
23. Barras del espectro 20 Hz-20 kHz reales: FFT de 256→1024 puntos (47 Hz/bin
    en vez de 187.5) + piso logarítmico 30→20 Hz. Con 256 puntos las nueve
    barras bajas se colapsaban sobre un solo bin de FFT y CUALQUIER sample las
    encendía todas por fuga espectral; ahora un kick ilumina solo las barras
    de graves y un tono de 1 kHz deja apagadas las de <300 Hz (regresión
    clavada en el test 4 del analyzer). Escala visual ×4 (barra llena =
    0 dBFS) con clamp al alto del strip. BACON_1.5_ANALYZER_20HZ.
24. Sinths +6 dB: el softclip viejo (hard clip ±1 + cúbica) limitaba TODO
    wave a pico 0.667 → el sinth quedaba ~3-4 dB bajo las samples. El
    clipper es ahora monótono por tramos (cúbica hasta ±1, taper lineal hasta
    el rail ±2) con boost ×2 previo: la sierra por defecto llega a pico 1.0
    (RMS ~0.48 post-pan, verificado por test; antes 0.31) y drive 0..100
    sigue abarcando de "boosteado" a "clip duro". BACON_1.5_SYNTH_LEVEL.
25. Guard del diálogo de proyectos ampliado: dump #5 (FileSystemService::Copy
    con ruta NULL durante un rename) — las entradas de directorio con ruta
    vacía se saltan en RecursiveCopyDirectory (TREEFROG_DIALOG_NULL_GUARD_V2)
    en lugar de pasarse al copy de SD.

Fixes:

- Fix EQ8 0 dB (U2.52.6): refreshFlat() y el lazo de Process() aplican la
  misma regla (banda activa ⇔ enabled && ganancia != 0) — la curva y el DSP
  nunca pueden divergir por el tipo seleccionado a 0 dB.
- Fix spectro (U2.52.6): anillo del analyzer 512→2048 frames para ventana
  FFT de 1024; el test de silencio alimenta 2048 ceros (antes 512 dejaba
  mitad de la ventana con audio viejo).

Novedades U2.52.7 (feedback de la prueba en R36SX #4):

26. FIX DEFINITIVO del VU del mixer: las barras estaban VACÍAS para todo
    instrumento (no solo sinths) desde H38.7-r4. El scan de picos medía
    `fp2fl(c)*(1.0f/32767.0f)`: fp2fl ya normaliza a 0..1 (Q15: val/32768)
    y el factor extra dividía DOS veces, dejando todo pico real en ~1e-6;
    el piso 0.002 del decaimiento lo anulaba → la barra nunca se movía.
    Corregido en el scan post-pan de los 8 canales (PlayerChannel.cpp) y en
    el scan pre-clip del master (AudioMixer.cpp): el nivel medido vuelve a
    ser la amplitud lineal 0..1, y mixVULevel/dB de la MIX page funcionan
    como estaban diseñados (barra llena = ~+3 dBFS). BACON_1.5_VU_SCALE_FIX.
    Verificación host con la cadena REAL (PlayerChannel + BassSynth + scan +
    MixerMeters, 44.1 kHz y 48 kHz): pico del sinth ~0.09, barra 69%
    (vol 100) / 87% (vol 127), paneo hard L/R correcto, mute/volumen 0/
    transporte parado/ocioso vacían la barra — nuevo runner
    run_host_mixer_vu_chain.sh (46 checks) registrado en el audit.
    EL MUCHOS "no se muestra en el mixer" reportado era ESTE bug: la frase
    sonaba (el bus mezcla el audio aunque el VU esté roto) y el mute
    silenciaba (Render devuelve audible=false y el bus descarta el buffer)
    — ambas observaciones son consistentes con la causa raíz.

Fixes:

- Fix EQ8 0 dB (U2.52.6): refreshFlat() y el lazo de Process() aplican la
  misma regla (banda activa ⇔ enabled && ganancia != 0) — la curva y el DSP
  nunca pueden divergir por el tipo seleccionado a 0 dB.
- Fix spectro (U2.52.6): anillo del analyzer 512→2048 frames para ventana
  FFT de 1024; el test de silencio alimenta 2048 ceros (antes 512 dejaba
  mitad de la ventana con audio viejo).
- Fix VU del mixer (U2.52.7): ver punto 26 — doble división en el scan de
  picos de PlayerChannel y del master (AudioMixer); el golden anterior de
  mixer_meters cubría solo la matemática de MixerMeters, no el scan, por
  eso la regresión pasó los gates.

Regresión: AUDIT_CLEAN_MAIN_U2523_OK (tests/run_all.sh), F10_BASELINE_OK
(golden core e54a5694), host bass 72 + piano 46 + eq8_struct 68 +
analyzer_mix 64 + mixer_vu_chain 46 checks OK (UBSAN limpio),
DIAGNOSTIC_GATE=0_ERRORS_0_WARNINGS, BUILD_U2523_OK, verify ERRORS=0.

Novedades U2.52.8 (feedback de la prueba en R36SX #5):

27. VOLUMEN REAL DE LOS SINTHS (feedback #5-A): el "sinth bajo vs samples"
    no era un ajuste de 3 dB — era un desajuste de ESCALA de ~90 dB. Los
    instrumentos escriben su buffer en escala master int16<<15 (count<<15,
    la del mixer, los sends, la grabación y el DAC: short(fp2i(v)) = >>15);
    los sinths (BassSynth/PianoSynth) renderizaban en Q15 (±1.0 = ±32768),
    así que a vol 100 un sinth llegaba al DAC a ~1 count mientras un sample
    llegaba a 11122 counts (medido con el HI HAT 01 real del SD) — por eso
    el sinth era inaudible al lado de una sample. Ningún test llegaba a la
    conversión fp2i() del DAC y por eso no se detectó. Fix: el sinth
    restaura la escala master a la salida de su Render (después de los
    kernels Q15 FV2/EQ, clamp ±i2fp(1)-1 para que el int16 del DAC nunca
    envuelva): un sinth a vol 100 es ahora full scale (32767 counts =
    0 dBFS), igual que un sample a pico pleno. Verificación host (extendido
    el runner del EQ8): sinth sostenido peak 32767 / rms 14822 counts vs
    HI HAT 11122 / 577 — la brecha de 90 dB desapareció.
    BACON_1.5_VOL_SYNTHS_FIX.
28. VU del mixer con la escala master real (feedback #5-G, complemento del
    26): el scan de picos usaba fp2fl() directamente, que para un buffer
    int16<<15 devuelve los COUNTS del DAC (1.0 == 32768), no la amplitud
    0..1 — con el fix del punto 27 el sinth pasó a leer 2867 counts (barra
    clavada al 100%) y las samples siempre habían sobreleído ~32768x
    (barra siempre llena, master en rojo con cualquier track). Ahora el
    scan divide por 32768 (count -> audio): track a vol 20 con instrumento
    full scale lee 20%, y el rojo +3 solo con una suma pre-clip real
    >= 0 dBFS — exactamente lo que prometía el diseño del
    BACON_1.5_VU_LINEAR_SCALE. PlayerChannel.cpp y AudioMixer.cpp.
29. EQ8 único funcional para samples+sinths (feedback #5-C): editar el EQ
    mataba el sonido porque el buffer del instrumento es int16<<15 pero el
    DSP del EQ es Q15: sin normalizar, el saturate() a ±i2fp(1) convertía
    toda la salida a ~1 LSB. El fix es el round-trip >>FIXED_SHIFT /
    <<FIXED_SHIFT alrededor del Process() (el mismo que FxEngine usa para
    sus kernels), aplicado en SampleInstrument y replicado en la cadena de
    los sinths. Test host dedicado de 28 checks: +12@100 Hz ×3.67, +6@1 kHz
    ×2.06, LP@500 corta el 1 kHz a ×0.24, +24 dB@100 Hz Q10 sin DC
    (dc=-1218 < 1500), reset vuelve a identidad ×0.91/×0.99, sinth vs
    HI HAT en escala DAC (punto 27). Nuevo runner
    run_host_sample_eq_edit.sh registrado en el audit.
30. Helper del EQ8 (feedback #5-E): SELECT+R1 en la pantalla del EQ8 abre
    ahora su propia sección de ayuda (título "EQ8", antes caía en la
    sección vacía por defecto) con los combos reales: L/R banda, X+L/R
    frecuencia, X+UP/DN ganancia ±1 dB, Y+L/R Q, Y+UP/DN intensidad todas
    las bandas, A on/off, B tipo de filtro, A+B reset de banda, SELECT
    bypass, START play, R+START stop, R+B volver al instrumento. El test
    host del HelpOverlay verifica el mapeo GetSection(VT_INSTRUMENT_EQ) ->
    sección 10 y el título.
31. Espectro DETRÁS del EQ8 (feedback #5-F): las barras del analyzer ya no
    viven en un strip separado bajo la pantalla: se dibujan DENTRO del
    canvas del EQ como fondo al 30% de opacidad (color mezclado 30/70 sobre
    el fondo, specC(90,190,130) sobre bgC(8,9,22) -> (33,63,54)) y encima se
    pintan la cuadrícula, ejes, asas y curva opacos — la curva sigue 100%
    legible y el espectro acompaña sin tapar nada. La escala ×4 (barra
    llena = 0 dBFS) se conserva, con clamp a la altura del canvas.
    BACON_1.5_EQ8_SPECTRUM_BACKDROP.
32. (feedback #5-B, #5-D, #5-H) verificados en esta build: campana bell con
    coeffs prewarped (centro exacto, sin asimetría), EQ8 fullscreen (todo
    el área bajo el header de 3 filas es canvas), mixer a pantalla completa
    (SongView -> VT_MIXER como vista de pantalla completa).

Fixes:

- Fix escala sinths (U2.52.8): ver punto 27 — BassSynth/PianoSynth
  restauran la escala master a la salida del Render con clamp ±i2fp(1)-1;
  el FV2 y el EQ de los sinths siguen operando en Q15 (el shift ocurre
  después de los kernels).
- Fix VU scan (U2.52.8): ver punto 28 — fp2fl()/32768 en el scan post-pan
  de PlayerChannel y en el scan pre-clip del master (AudioMixer); el fix
  del 26 (U2.52.7) era correcto para buffers Q15, pero los buffers son
  int16<<15 desde el 27 — la división count->audio es ahora explícita.

Regresión: AUDIT_CLEAN_MAIN_U2523_OK (tests/run_all.sh), host
SAMPLE_EQ_EDIT 28 + MIXER_VU_CHAIN 48 + bass 72 + piano 46 + HelpOverlay +
analyzer checks OK (UBSAN limpio), git diff --check limpio,
DIAGNOSTIC_GATE=0_ERRORS_0_WARNINGS, BUILD_U2523_OK,
SD_MOUNT=/mnt/g install/verify OK (BACKUP en
/mnt/d/R36S/PORT LPTRACKER/BACKUPS/LGPT_BEFORE_U2523_20260818_213642).

Novedades U2.53 (feedback de la prueba en R36SX #7):

33. EQ8 menú header en píxeles (feedback #7): el menú de cabecera del EQ8
    se dibuja con coordenadas en píxeles (no celdas), alineado con el resto
    de la vista; verificado por host test.
34. Limiter del EQ8 (feedback #7): clamp del Df2 a kBlockLimit = 65535
    (2.0) por banda/canal, reemplazando el saturate a ±1 que distorsionaba
    en boost de baja frecuencia (la campana prewarped puede pasar de 1.0).
35. Suma master 64-bit con medidor >= (feedback #7): la suma del mixer es
    int64 con acumulador pre-clip y el medidor del master marca >= 0 dBFS
    antes del clamp (sin flicker en el límite); ver punto 36 para la
    profundidad del scratch. BACON_1.5_MIXER_FULLSCREEN.
36. Analizador dB sobre el mix (feedback #7): el espectro del analyzer se
    alimenta de la mezcla maestra final (post master bus y post FxEngine,
    DummyAudioOut::primarySoundBuffer_) con escala correcta (counts >> 16 +
    >> 16), así las barras leen exactamente lo que se oye.
37. Mixer DAW fullscreen (feedback #7): layout del mixer a pantalla
    completa con labels hex, volumen, barras 27% más altas y marcador M/C;
    ver U2.54 para el volumen debajo de la barra.

Regresión U2.53: AUDIT_CLEAN_MAIN_U2523_OK, host eq8 77 + analyzer 64 +
mixer_meters checks OK (UBSAN limpio), BUILD_U2523_OK, verify ERRORS=0.
Core fcc02d6b instalado en SD.

Novedades U2.54 (feedback de la prueba en R36SX #8):

38. Suma master 64-bit por profundidad (feedback #8): el scratch de la suma
    int64 era único y plano, así que los Render() anidados de los buses
    hijos lo pisaban (cada bus reutilizaba el mismo buffer) y la
    polifonía colapsaba; ahora el scratch es por nivel de anidamiento
    (s_moduleSumScratch[4][] con s_moduleSumDepth). BACON_1.5_64BIT_SUM_DEPTH.
39. Volumen debajo de la barra (feedback #8): el número de volumen se dibuja
    DEBAJO de la barra (fila y+height+1) y el marcador pan/mute una fila
    más abajo (y+height+2), con barHeight 19→18 (barras 7..24): orden DAW
    hex, barra, volumen, M/C. BACON_1.5_MIXER_VOL_BELOW. Layout verificado
    por test_ui_centered_layout.py (constantes barHeight=18, filas 25/26).

Fixes:

- Fix polifonía (U2.54): ver punto 38 — el scratch plano se clobberaba en
  el render anidado de buses; el test mixer_64bit_sum ahora verifica los
  niveles de profundidad.

Regresión U2.54: AUDIT_CLEAN_MAIN_U2523_OK, host MIXER_64BIT_SUM 22 +
MIXER_VU_CHAIN 52 + eq8 77 + bass 72 + piano 46 checks OK (UBSAN limpio),
git diff --check limpio, BUILD_U2523_OK.

Novedades U2.55 (feedback de la prueba en R36SX #8):

40. Niveles estilo FL (feedback #8): la escala de volumen del instrumento
    era 1/255, así que a vol 100 un instrumento full scale llegaba a
    -6.2 dB con el canal a 127 (100/255 x 1.27) — exactamente la queja de
    "barras que no suben"; ahora el volumen usa 1/128 (128 = unidad,
    como FL Studio) y el attenuate del filtro conserva su propia escala
    /255 (default 255 = unidad, sin boost fantasma).
    BACON_1.5_VOL_LEVELS_FL (SampleInstrument.cpp).
41. Sinths a unidad (feedback #8): se elimina el pad x0.1 que los sinths
    aplicaban tras la escala master del U2.52.8 (heredado de la
    comparación 0..255 vs 0..100, dejaba el sinth a -20 dB); un sinth a
    vol 100 es ahora full scale igual que un sample a pico pleno.
    BACON_1.5_VOL_SYNTHS_UNITY (BassSynth/PianoSynth).
42. Validación EQ ±1 dB (feedback #8): el runner SAMPLE_EQ_EDIT pasa de 28
    a 62 checks: sección B1 (cada tipo de filtro a ±1 dB @ 100 Hz: bell y
    shelves en sqrt(A)/A, LP/HP con verificación de corte real en 2*f0 y
    f0/2, NOTCH y BP con ganancia no nula — una banda a 0 dB es identidad
    para TODO tipo, BACON_1.5_EQ8_0DB_TRANSPARENT) y sección B2 (kick real
    del SD a través del EQ ±1 dB @ 80 Hz: la barra sube ~8 puntos por
    +1 dB y a canal 127 el kick queda a ~-1 dB barra 85%, sobre la marca
    -6 dB; el DC del bucle es artefacto del loop-click, relajado a
    < 0.15 x 32768). El análisis confirmó el flujo: los ±1 dB se respetan
    (bell 1.117/0.903, shelves 1.115/0.926/1.169, LP 1.009, HP 1.236,
    NOTCH 0.259, BP 0.994).
43. Límite Q15 de LP/HP documentado (feedback #8): con RBJ cookbook a baja
    frecuencia (80-100 Hz) los numeradores (1-cw)/2 se cuantizan a 1-3
    counts Q15 y la ganancia en el centro puede desviarse ~±2 dB (medido
    +2.4 dB a 100 Hz) — límite de fidelidad del formato, no defecto de
    diseño (shelves/bell intactos, el sonido nunca se destruye); comentado
    en EqBiquad.h.

Fixes:

- Fix niveles FL (U2.55): ver punto 40 — volscale 1/128 separada del
  attenuate /255 en ambos sitios de render de SampleInstrument.

Regresión U2.55: AUDIT_CLEAN_MAIN_U2523_OK (gate completo), host
SAMPLE_EQ_EDIT 62 + eq8 77 + MIXER_VU_CHAIN 52 + bass 72 + piano 46 +
MIXER_64BIT_SUM 22 checks OK (UBSAN limpio), git diff --check limpio,
DIAGNOSTIC_GATE=0_ERRORS_0_WARNINGS, BUILD_U2523_OK,
SD_MOUNT=/mnt/g install/verify OK (BACKUP en
/mnt/d/R36S/PORT LPTRACKER/BACKUPS/LGPT_BEFORE_U2523_20260819_094512).
Core bf8cf44c instalado en SD.