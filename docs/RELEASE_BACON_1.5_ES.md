Pre-release Bacon 1.5 - Sinths and EQ8 (build U2.52.3, 2026-08-18)

Build:
- lgpt_r36sx_u2523.so - SHA256 9faaa7134204832e0fe9aa3de47525257b3bdceb848c03cedb752a6741418f72
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

Regresión: AUDIT_CLEAN_MAIN_U2523_OK (tests/run_all.sh), F10_BASELINE_OK
(golden core 9faaa713), host bass 45 + piano 46 + eq8_struct 63 +
analyzer_mix 55 checks OK, TEST_FX_PHASE19_AUDITION_ISOLATED_OK,
DIAGNOSTIC_GATE=0_ERRORS_0_WARNINGS, BUILD_U2523_OK, verify ERRORS=0.