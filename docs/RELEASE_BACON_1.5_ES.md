Pre-release Bacon 1.5 - Sinths and EQ8 (build U2.52.3, 2026-08-18)

Build:
- lgpt_r36sx_u2523.so - SHA256 739d35296c8e0b10e5cec8d851f47093580a795ccf0056f70d47c7edecf04689
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
(golden core 9faaa713), host bass 72 + piano 46 + eq8_struct 63 +
analyzer_mix 55 checks OK (bass 7b con UBSAN limpio), TEST_FX_PHASE19
_AUDITION_ISOLATED_OK, DIAGNOSTIC_GATE=0_ERRORS_0_WARNINGS, BUILD_U2523_OK,
verify ERRORS=0.